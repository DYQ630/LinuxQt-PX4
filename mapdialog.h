/**
 * @file mapdialog.h
 * @brief 地图弹窗类头文件
 * @details 使用 QWebEngineView 内嵌高德地图 JS API，实现以下功能：
 *          - 显示卫星地图 + 路网图层（基于高德地图 API）
 *          - 实时位置更新（通过 runJavaScript 调用 JS 端 setPosition）
 *          - 路线规划显示（起点/终点标记）
 *          - 手动坐标校准（输入经纬度定位到指定位置）
 *          - GPS 定位按钮（预留接口）
 *          - 坐标有效性校验（NaN/Inf/范围检查）
 *          - 双重安全网机制（2秒检查 + 5秒强制就绪）
 */

#ifndef MAPDIALOG_H
#define MAPDIALOG_H

#include <QMainWindow>
#include <QWebEngineView>
#include <QWebEnginePage>
#include <QWebEngineSettings>
#include <QLineEdit>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QEvent>
#include <QMouseEvent>
#include <QCloseEvent>

/**
 * @class DebugWebPage
 * @brief 自定义 WebEnginePage，用于将 JavaScript 控制台日志转发到 Qt qDebug
 * @details 重写 javaScriptConsoleMessage 方法，过滤与高德地图相关的日志，
 *          方便调试地图加载和位置更新问题
 */
class DebugWebPage : public QWebEnginePage
{
    Q_OBJECT
public:
    explicit DebugWebPage(QObject *parent = nullptr) : QWebEnginePage(parent) {}

protected:
    void javaScriptConsoleMessage(JavaScriptConsoleMessageLevel level,
                                  const QString &message,
                                  int lineNumber,
                                  const QString &sourceID) override;
};

/**
 * @class MapDialog
 * @brief 地图弹窗对话框，显示高德卫星地图并支持实时定位
 * @details 核心功能：
 *          1. 内嵌 amap.html 页面（基于高德 JS API）
 *          2. 通过 setPosition/updatePosition 接收 C++ 端位置更新
 *          3. 通过 runJavaScript 与 JS 端通信
 *          4. 支持路线规划模式（起点→终点）
 *          5. 提供手动坐标校准界面
 */
class MapDialog : public QMainWindow
{
    Q_OBJECT

public:
    explicit MapDialog(QWidget *parent = nullptr);
    ~MapDialog();

    /**
     * @brief 设置地图位置（普通模式，新位置会立即应用）
     * @param lon 经度
     * @param lat 纬度
     * @details 用于首次设置位置或手动定位
     */
    void setPosition(double lon, double lat);
    
    /**
     * @brief 更新地图位置（实时跟随模式）
     * @param lon 经度
     * @param lat 纬度
     * @details 用于定时更新当前位置（如每秒调用一次），仅在非路线模式且页面就绪时生效
     */
    void updatePosition(double lon, double lat);
    
    /**
     * @brief 设置路线模式（显示起点到终点的路径）
     * @param startLon 起点经度
     * @param startLat 起点纬度
     * @param endLon 终点经度
     * @param endLat 终点纬度
     */
    void setRoute(double startLon, double startLat, double endLon, double endLat);

signals:
    void closed();  // 窗口关闭时发射，供父窗口恢复positionWebView显示

protected:
    void showEvent(QShowEvent *event) override;   // 窗口显示时强制刷新地图
    void hideEvent(QHideEvent *event) override;   // 窗口隐藏时清除路线模式
    void closeEvent(QCloseEvent *event) override; // 窗口关闭时发射closed信号
    bool eventFilter(QObject *obj, QEvent *event) override;  // 事件过滤器，拦截拖拽

private slots:
    void onPageLoadFinished(bool ok);             // HTML 加载完成回调，启动就绪检查
    void onUrlChanged(const QUrl &url);           // URL 变化回调（调试用）
    void onManualLocate();                         // 手动定位按钮槽函数
    void onUseGpsPosition();                       // 使用 GPS 位置按钮槽函数

private:
    QWebEngineView *webView;       // 地图视图控件（嵌入 amap.html）
    DebugWebPage *page;            // 自定义页面（用于 JS 日志转发）

    // 手动校准控件
    QLineEdit *leLon;              // 经度输入框（手动定位用）
    QLineEdit *leLat;              // 纬度输入框（手动定位用）
    QLabel *lblGpsStatus;          // GPS 状态显示标签

    double currentLon;             // 当前经度
    double currentLat;             // 当前纬度
    bool isRouteMode;              // 是否为路线模式（true=显示路线，false=普通定位）
    bool pageReady;                // 地图页面是否就绪

    double routeStartLon;          // 路线起点经度
    double routeStartLat;          // 路线起点纬度
    double routeEndLon;            // 路线终点经度
    double routeEndLat;            // 路线终点纬度

    int retryCount;                // 当前重试次数
    static const int MAX_RETRIES = 30;  // 最大重试次数（30次 × 300ms = 9秒）

    double lastSentLon;            // 上次发送到JS的经度（用于去重）
    double lastSentLat;            // 上次发送到JS的纬度（用于去重）
    static const double MIN_SEND_DIFF;  // 最小发送差值（约1米精度）

    void checkMapReady();          // 检查地图是否就绪并应用位置/路线
    void applyPosition();          // 应用位置到地图
    void applyRoute();             // 应用路线到地图
    void runJavaScript(const QString &js);  // 执行 JS 代码（封装函数）
    void buildUi();                // 构建 UI 界面
    static bool isCoordValid(double lon, double lat);  // 校验坐标有效性
};

#endif // MAPDIALOG_H
