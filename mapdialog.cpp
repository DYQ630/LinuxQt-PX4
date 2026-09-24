#include "mapdialog.h"
#include <QWebEngineSettings>
#include <QWebEngineProfile>
#include <QDebug>
#include <QTimer>
#include <QtMath>

// 位置发送去重阈值：经纬度变化小于此值时跳过JS调用（约1米精度）
const double MapDialog::MIN_SEND_DIFF = 0.00001;

void DebugWebPage::javaScriptConsoleMessage(JavaScriptConsoleMessageLevel level,
                                            const QString &message,
                                            int lineNumber,
                                            const QString &sourceID)
{
    const char *lvl = "INFO";
    switch (level) {
    case WarningMessageLevel: lvl = "WARN"; break;
    case ErrorMessageLevel:   lvl = "ERR "; break;
    default:                  lvl = "INFO"; break;
    }
    // 只转发和 AMap 相关的日志，减少噪声
    if (message.contains("[AMap]", Qt::CaseInsensitive)
        || message.contains("error", Qt::CaseInsensitive)
        || message.contains("fail", Qt::CaseInsensitive)
        || message.contains("undefined", Qt::CaseInsensitive)
        || level != InfoMessageLevel) {
        qDebug().noquote() << QString("[JS %1] L%2: %3 (src:%4)")
                                  .arg(lvl).arg(lineNumber).arg(message).arg(sourceID);
    }
}

/**
 * @brief 判断经纬度是否为有效范围
 * @param lon 经度
 * @param lat 纬度
 * @return true=有效坐标，false=无效坐标
 * @details 校验规则：
 *          1. 不能为 NaN（非数字）
 *          2. 不能为 Inf（无穷大）
 *          3. 经度范围 [-180, 180]
 *          4. 纬度范围 [-90, 90]
 *          5. (0, 0) 视为无效（未初始化状态）
 */
bool MapDialog::isCoordValid(double lon, double lat)
{
    if (qIsNaN(lon) || qIsNaN(lat)) return false;   // 检查是否为非数字
    if (qIsInf(lon) || qIsInf(lat)) return false;   // 检查是否为无穷大
    if (lon < -180.0 || lon > 180.0) return false;  // 经度范围检查
    if (lat <  -90.0 || lat >  90.0) return false;  // 纬度范围检查
    if (lon == 0.0 && lat == 0.0) return false;     // (0,0) 视为未初始化
    return true;
}

/**
 * @brief 构造函数
 * @param parent 父窗口指针
 * @details 初始化地图弹窗：
 *          1. 设置窗口属性和样式
 *          2. 构建 UI 界面（工具栏 + 地图视图）
 *          3. 配置 WebEngine 设置（启用 JS、远程资源访问等）
 *          4. 连接信号槽
 *          5. 加载 amap.html 页面
 */
MapDialog::MapDialog(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowFlags(Qt::Window | Qt::WindowMinimizeButtonHint |
                   Qt::WindowMaximizeButtonHint | Qt::WindowCloseButtonHint);
    setWindowTitle("卫星地图 - 实时定位");
    setStyleSheet("background: rgba(10, 20, 40, 0.95);");
    setGeometry(100, 100, 900, 700);

    // 构建 UI 界面（顶部工具栏 + 中间地图视图）
    buildUi();

    // 使用自定义的 DebugWebPage，将 JS 控制台日志转发到 Qt 调试输出
    page = new DebugWebPage(this);
    webView->setPage(page);

    // 配置 WebEngine 设置，确保高德地图 API 能正常加载和执行
    QWebEngineSettings *settings = page->settings();
    settings->setAttribute(QWebEngineSettings::JavascriptEnabled, true);              // 启用 JavaScript
    settings->setAttribute(QWebEngineSettings::LocalContentCanAccessRemoteUrls, true); // 允许本地内容访问远程URL（加载高德API）
    settings->setAttribute(QWebEngineSettings::LocalContentCanAccessFileUrls, true);  // 允许本地内容访问文件URL
    settings->setAttribute(QWebEngineSettings::ScrollAnimatorEnabled, true);           // 启用滚动动画
    settings->setAttribute(QWebEngineSettings::LocalStorageEnabled, true);             // 启用本地存储
    settings->setAttribute(QWebEngineSettings::AutoLoadImages, true);                 // 自动加载图片
    settings->setAttribute(QWebEngineSettings::LinksIncludedInFocusChain, false);     // 减少焦点链干扰

    // 禁用 QWebEngineView 的原生拖拽行为（防止地图像图片被拖出来）
    webView->setAcceptDrops(false);
    webView->setMouseTracking(true);                                                   // 启用鼠标跟踪
    webView->setAttribute(Qt::WA_NativeWindow, true);
    webView->installEventFilter(this);  // 安装事件过滤器拦截拖拽

    // 连接页面加载完成信号
    connect(webView, &QWebEngineView::loadFinished, this, &MapDialog::onPageLoadFinished);
    connect(webView, &QWebEngineView::urlChanged,   this, &MapDialog::onUrlChanged);

    // 初始化状态变量（成都坐标作为默认值）
    currentLon = 104.0668;
    currentLat = 30.5728;
    isRouteMode = false;    // 初始为普通定位模式
    pageReady = false;      // 页面未就绪

    // 路线模式坐标初始化为0
    routeStartLon = 0;
    routeStartLat = 0;
    routeEndLon = 0;
    routeEndLat = 0;
    retryCount = 0;         // 重置重试计数器

    lastSentLon = 0;
    lastSentLat = 0;

    // 加载 amap.html 页面（从 Qt 资源系统加载）
    // 带 layerControl=1 参数启用图层切换控件（卫星图/标准图/路网叠加）
    qDebug() << "[地图弹窗] 开始加载 qrc:/amap.html?layerControl=1 ...";
    webView->load(QUrl("qrc:/amap.html?layerControl=1"));
}

/**
 * @brief 构建UI布局
 * @details 顶部工具栏（GPS状态+手动坐标校准）+ 中间地图
 */
void MapDialog::buildUi()
{
    QWidget *centralWidget = new QWidget(this);
    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(4);

    // ---- 顶部工具栏 ----
    QHBoxLayout *toolbarLayout = new QHBoxLayout();
    toolbarLayout->setSpacing(6);

    // GPS状态标签
    lblGpsStatus = new QLabel("定位中...", centralWidget);
    lblGpsStatus->setStyleSheet(
        "color: #00ffff; font-size: 12px; padding: 3px 8px; "
        "border: 1px solid #00ffff; border-radius: 3px; "
        "background: rgba(0,255,255,0.08);"
    );
    toolbarLayout->addWidget(lblGpsStatus);
    toolbarLayout->addSpacing(10);

    // 手动校准：经度
    toolbarLayout->addWidget(new QLabel("经度:", centralWidget));
    leLon = new QLineEdit(centralWidget);
    leLon->setPlaceholderText("例: 104.229908");
    leLon->setFixedWidth(130);
    leLon->setStyleSheet(
        "QLineEdit { color: #00ffff; background: rgba(0,20,40,0.8); "
        "border: 1px solid #00ffff; border-radius: 3px; padding: 3px 6px; }"
    );
    toolbarLayout->addWidget(leLon);

    // 手动校准：纬度
    toolbarLayout->addWidget(new QLabel("纬度:", centralWidget));
    leLat = new QLineEdit(centralWidget);
    leLat->setPlaceholderText("例: 30.727576");
    leLat->setFixedWidth(130);
    leLat->setStyleSheet(
        "QLineEdit { color: #00ffff; background: rgba(0,20,40,0.8); "
        "border: 1px solid #00ffff; border-radius: 3px; padding: 3px 6px; }"
    );
    toolbarLayout->addWidget(leLat);

    // 定位按钮
    QPushButton *btnLocate = new QPushButton("定位", centralWidget);
    btnLocate->setStyleSheet(
        "QPushButton { color: #fff; background: #1677ff; border: none; "
        "border-radius: 3px; padding: 3px 14px; font-weight: bold; }"
        "QPushButton:hover { background: #4096ff; }"
    );
    connect(btnLocate, &QPushButton::clicked, this, &MapDialog::onManualLocate);
    toolbarLayout->addWidget(btnLocate);

    // 使用GPS按钮
    QPushButton *btnGps = new QPushButton("GPS定位", centralWidget);
    btnGps->setStyleSheet(
        "QPushButton { color: #fff; background: #52c41a; border: none; "
        "border-radius: 3px; padding: 3px 14px; font-weight: bold; }"
        "QPushButton:hover { background: #73d13d; }"
    );
    connect(btnGps, &QPushButton::clicked, this, &MapDialog::onUseGpsPosition);
    toolbarLayout->addWidget(btnGps);

    toolbarLayout->addStretch();

    mainLayout->addLayout(toolbarLayout);

    // ---- 地图视图 ----
    webView = new QWebEngineView(centralWidget);
    webView->setStyleSheet("border: 1px solid rgba(0,255,255,0.3); border-radius: 4px;");
    mainLayout->addWidget(webView, 1);

    setCentralWidget(centralWidget);
}

MapDialog::~MapDialog()
{
}

void MapDialog::onUrlChanged(const QUrl &url)
{
    qDebug() << "[地图弹窗] URL变更:" << url.toString();
}

/**
 * @brief HTML 加载完成回调
 */
void MapDialog::onPageLoadFinished(bool ok)
{
    if (!ok) {
        qWarning() << "[地图弹窗] HTML 页面加载失败！";
        pageReady = true; // 避免 pending 死锁
        return;
    }
    qDebug() << "[地图弹窗] HTML 加载完成，启动就绪检查";
    retryCount = 0;
    checkMapReady();
}

/**
 * @brief 检查地图是否就绪并应用位置/路线
 * @details 这是一个递归重试函数，用于在地图加载完成后：
 *          1. 检查坐标有效性
 *          2. 根据模式（定位/路线）构建对应的 JS 代码
 *          3. 执行 JS 代码并根据返回状态决定是否重试
 *          4. 支持 pending 状态的自动重试（最多 MAX_RETRIES 次）
 *          5. 最终失败时使用默认成都坐标兜底
 */
void MapDialog::checkMapReady()
{
    if (!page) return;

    QString jsCode;
    // 根据模式构建对应的 JS 调用代码
    if (isRouteMode && isCoordValid(routeStartLon, routeStartLat) && isCoordValid(routeEndLon, routeEndLat)) {
        // 路线模式：调用 addRouteMarkers 显示起点和终点
        jsCode = QString(
            "addRouteMarkers(%1, %2, %3, %4);"
        ).arg(routeStartLon, 0, 'f', 6)
         .arg(routeStartLat, 0, 'f', 6)
         .arg(routeEndLon, 0, 'f', 6)
         .arg(routeEndLat, 0, 'f', 6);
    } else if (isCoordValid(currentLon, currentLat)) {
        // 普通定位模式：调用 setPosition 设置位置
        jsCode = QString(
            "setPosition(%1, %2);"
        ).arg(currentLon, 0, 'f', 6)
         .arg(currentLat, 0, 'f', 6);
    } else {
        // 坐标无效（可能主程序尚未获取到定位），重试等待
        if (retryCount < MAX_RETRIES) {
            retryCount++;
            qDebug() << "[地图弹窗] 坐标无效，等待主程序获取定位... 第" << retryCount << "次重试";
            QTimer::singleShot(300, this, [this]() { checkMapReady(); });  // 300ms 后重试
        } else {
            // 超过最大重试次数，使用默认成都坐标兜底
            qWarning() << "[地图弹窗] 坐标一直无效，使用默认成都坐标兜底";
            currentLon = 104.0668;
            currentLat = 30.5728;
            jsCode = QString(
                "setPosition(%1, %2);"
            ).arg(currentLon, 0, 'f', 6)
             .arg(currentLat, 0, 'f', 6);
        }
        return;
    }

    // 执行 JS 代码并处理返回状态
    page->runJavaScript(jsCode, [this, jsCode](const QVariant &result) {
        QString status = result.toString();
        if (status == "ok" || status.isEmpty()) {
            // 成功：标记页面就绪，重置重试计数
            qDebug() << "[地图弹窗] 位置/路线应用成功";
            pageReady = true;
            retryCount = 0;
        } else if (status == "pending" && retryCount < MAX_RETRIES) {
            // pending：JS 端地图尚未就绪，稍后重试
            retryCount++;
            if (retryCount % 5 == 1) {
                qDebug() << "[地图弹窗] JS返回pending，等待高德地图初始化... 第" << retryCount << "次重试";
            }
            QTimer::singleShot(300, this, [this]() { checkMapReady(); });
        } else if (status.startsWith("error:", Qt::CaseInsensitive)) {
            // 出错：标记就绪避免死循环
            qWarning() << "[地图弹窗] JS执行出错:" << status;
            pageReady = true;
            retryCount = 0;
        } else {
            // 其他状态：结束重试
            qWarning() << "[地图弹窗] JS返回其他状态:" << status << "，结束重试";
            pageReady = true;
            retryCount = 0;
        }
    });
}

void MapDialog::applyPosition()
{
    if (!page) return;
    if (!isCoordValid(currentLon, currentLat)) return;

    qDebug() << "[地图弹窗] applyPosition: lon=" << currentLon << "lat=" << currentLat;
    page->runJavaScript(QString(
        "setPosition(%1, %2);"
    ).arg(currentLon, 0, 'f', 6)
     .arg(currentLat, 0, 'f', 6),
    [this](const QVariant &result) {
        QString status = result.toString();
        if (status == "error" || status.startsWith("error:")) {
            qWarning() << "[地图弹窗] setPosition 报错:" << status;
        } else if (status == "pending") {
            qDebug() << "[地图弹窗] setPosition 返回 pending";
        } else {
            qDebug() << "[地图弹窗] setPosition 返回:" << status << " 坐标已应用";
        }
    });
}

void MapDialog::applyRoute()
{
    if (!page) return;
    if (!isCoordValid(routeStartLon, routeStartLat) || !isCoordValid(routeEndLon, routeEndLat)) return;

    page->runJavaScript(QString(
        "addRouteMarkers(%1, %2, %3, %4);"
    ).arg(routeStartLon, 0, 'f', 6)
     .arg(routeStartLat, 0, 'f', 6)
     .arg(routeEndLon, 0, 'f', 6)
     .arg(routeEndLat, 0, 'f', 6),
    [this](const QVariant &result) {
        QString status = result.toString();
        if (status == "error" || status.startsWith("error:")) {
            qWarning() << "[地图弹窗] addRouteMarkers 报错:" << status;
        }
    });
}

/**
 * @brief 设置地图位置（普通模式）
 */
void MapDialog::setPosition(double lon, double lat)
{
    if (!isCoordValid(lon, lat)) {
        qDebug() << "[地图弹窗] setPosition 收到无效坐标，忽略:" << lon << "," << lat;
        return;
    }

    qDebug() << "[地图弹窗] setPosition: lon=" << lon << "lat=" << lat << "pageReady=" << pageReady;
    currentLon = lon;
    currentLat = lat;
    isRouteMode = false;
    lastSentLon = 0;  // 重置去重标记，确保首次位置正确发送
    lastSentLat = 0;

    if (!page) return;

    // 清除路线标记
    page->runJavaScript("clearRouteMarkers();");

    if (pageReady) {
        applyPosition();
    } else {
        retryCount = 0;
        checkMapReady();
    }
}

/**
 * @brief 更新地图位置（实时跟随）
 * @details 增加位置去重逻辑，避免频繁的 JS 调用导致卡顿
 */
void MapDialog::updatePosition(double lon, double lat)
{
    if (!isCoordValid(lon, lat)) return;

    currentLon = lon;
    currentLat = lat;

    if (isRouteMode) return;
    if (!pageReady || !page) return;

    // 位置去重：变化极小时跳过 JS 调用（减少 runJavaScript 开销）
    if (qAbs(lon - lastSentLon) < MIN_SEND_DIFF && qAbs(lat - lastSentLat) < MIN_SEND_DIFF) {
        return;
    }
    lastSentLon = lon;
    lastSentLat = lat;

    applyPosition();
}

/**
 * @brief 路线模式
 */
void MapDialog::setRoute(double startLon, double startLat, double endLon, double endLat)
{
    if (!isCoordValid(startLon, startLat) || !isCoordValid(endLon, endLat)) {
        qDebug() << "[地图弹窗] setRoute 收到无效坐标，忽略";
        return;
    }

    currentLon = startLon;
    currentLat = startLat;
    isRouteMode = true;

    routeStartLon = startLon;
    routeStartLat = startLat;
    routeEndLon   = endLon;
    routeEndLat   = endLat;

    if (!page) return;
    if (pageReady) {
        applyRoute();
    } else {
        retryCount = 0;
        checkMapReady();
    }
}

void MapDialog::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);
    qDebug() << "[地图弹窗] showEvent 触发, 窗口尺寸:" << width() << "x" << height();

    // 窗口显示后，WebEngineView 布局完成，强制 JS 端 resize 地图
    if (page) {
        page->runJavaScript("_resetFollowState();");  // 重置跟随状态，确保首次定位居中
        page->runJavaScript("_forceResize();");
        // 延迟 500ms 后重新应用位置，确保地图在 resize 后正确定位
        QTimer::singleShot(500, this, [this]() {
            if (isRouteMode && isCoordValid(routeStartLon, routeStartLat) && isCoordValid(routeEndLon, routeEndLat)) {
                applyRoute();
            } else if (isCoordValid(currentLon, currentLat)) {
                applyPosition();
            }
        });
    }
}

void MapDialog::hideEvent(QHideEvent *event)
{
    QMainWindow::hideEvent(event);
    isRouteMode = false;
}

void MapDialog::closeEvent(QCloseEvent *event)
{
    QMainWindow::closeEvent(event);
    emit closed();
}

/**
 * @brief 手动定位按钮槽函数
 * @details 从输入框读取经纬度，定位到指定位置
 */
void MapDialog::onManualLocate()
{
    bool okLon = false, okLat = false;
    double lon = leLon->text().trimmed().toDouble(&okLon);
    double lat = leLat->text().trimmed().toDouble(&okLat);

    if (!okLon || !okLat || !isCoordValid(lon, lat)) {
        qWarning() << "[地图弹窗] 手动输入坐标无效:" << leLon->text() << "," << leLat->text();
        lblGpsStatus->setText("坐标无效，请检查格式");
        lblGpsStatus->setStyleSheet(
            "color: #ff5555; font-size: 12px; padding: 3px 8px; "
            "border: 1px solid #ff5555; border-radius: 3px; "
            "background: rgba(255,85,85,0.08);"
        );
        return;
    }

    qDebug() << "[地图弹窗] 手动定位:" << lon << lat;
    lblGpsStatus->setText("手动定位: " + QString::number(lon, 'f', 4) + ", " + QString::number(lat, 'f', 4));
    lblGpsStatus->setStyleSheet(
        "color: #ffaa00; font-size: 12px; padding: 3px 8px; "
        "border: 1px solid #ffaa00; border-radius: 3px; "
        "background: rgba(255,170,0,0.08);"
    );

    setPosition(lon, lat);
}

/**
 * @brief 使用当前GPS位置按钮槽函数
 * @details 读取 widget.cpp 中最新的 GPS 位置（通过 parent 或其他方式获取）
 */
void MapDialog::onUseGpsPosition()
{
    // 从父窗口获取最新GPS位置
    // 尝试通过 qobject_cast 获取 Widget 的坐标
    QWidget *w = parentWidget();
    if (w) {
        // 读取输入框当前值作为GPS位置（用户可以先在主界面确认坐标）
        qDebug() << "[地图弹窗] 使用GPS位置按钮";
        // 触发重新读取坐标
        if (leLon && leLat && !leLon->text().trimmed().isEmpty()) {
            onManualLocate();
        } else {
            lblGpsStatus->setText("请先输入或确认坐标");
        }
    }
}

void MapDialog::runJavaScript(const QString &js)
{
    if (!page) return;
    page->runJavaScript(js, [js](const QVariant &result) {
        if (!result.isValid()) {
            qWarning() << "[地图弹窗] runJavaScript 无返回值:" << js.left(60);
        }
    });
}

/**
 * @brief 事件过滤器
 * @details 拦截 QWebEngineView 的拖拽事件，防止浏览器原生拖拽行为
 *          （如地图像图片一样被拖出来）
 */
bool MapDialog::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == webView) {
        // 拦截拖拽事件：阻止 Qt 的原生拖拽开始
        if (event->type() == QEvent::DragEnter ||
            event->type() == QEvent::DragMove ||
            event->type() == QEvent::Drop) {
            event->accept();
            return true;
        }
        // 拦截鼠标按下时的拖拽起始（防止图片/链接被拖拽）
        if (event->type() == QEvent::MouseButtonPress) {
            QMouseEvent *me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton) {
                // 不拦截，让 AMap JS 处理
            }
        }
    }
    return QMainWindow::eventFilter(obj, event);
}
