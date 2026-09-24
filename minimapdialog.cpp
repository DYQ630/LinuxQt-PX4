/**
 * @file minimapdialog.cpp
 * @brief 小地图弹窗类实现文件
 * @details 该文件实现了MiniMapDialog类的所有成员函数，包括地图加载、
 *          缩放、拖动和标记点绘制等功能。使用高德地图瓦片API加载地图。
 */

// 包含MiniMapDialog类的头文件
#include "minimapdialog.h"
#include <QDebug>   // 包含QDebug类头文件，用于调试输出
#include <cmath>    // 包含cmath库，用于数学计算（pow、M_PI等）

/**
 * @brief MiniMapDialog类构造函数
 * @details 初始化窗口标题、大小、布局和成员变量，设置非模态对话框属性
 * @param parent 父窗口指针，默认为nullptr
 */
MiniMapDialog::MiniMapDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle("位置地图");          // 设置窗口标题为"位置地图"
    setFixedSize(320, 280);             // 设置固定大小320x280像素
    setModal(false);                    // 设置为非模态对话框，避免阻塞输入框
    setFocusPolicy(Qt::NoFocus);        // 不接受焦点，避免抢夺输入框焦点
    setAttribute(Qt::WA_TransparentForMouseEvents, false);  // 允许鼠标事件
    
    // 关键：设置WindowStaysOnTopHint，确保小地图弹窗能浮于主界面的WebEngineView地图之上
    // （因为positionWebView是独立的顶层置顶窗口，普通QDialog可能会被遮挡）
    setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint);
    setWindowFlag(Qt::WindowContextHelpButtonHint, false);  // 隐藏帮助按钮
    
    // ==================== 创建布局 ====================
    QVBoxLayout *layout = new QVBoxLayout(this);  // 创建垂直布局对象
    layout->setContentsMargins(5, 5, 5, 5);       // 设置布局外边距：上下左右各5像素
    layout->setSpacing(5);                         // 设置控件间距为5像素
    
    // ==================== 创建坐标信息标签 ====================
    coordLabel = new QLabel(this);                 // 创建坐标显示标签
    coordLabel->setAlignment(Qt::AlignCenter);     // 设置文字居中对齐
    coordLabel->setStyleSheet("color: #00ffff; font-size: 11px; background: #0a1628; padding: 2px;");  // 设置样式：青色文字、深蓝背景
    coordLabel->setText("正在加载地图...");         // 设置初始提示文字
    
    // ==================== 创建地图显示标签 ====================
    mapLabel = new QLabel(this);                   // 创建地图显示标签
    mapLabel->setStyleSheet("border: 1px solid #00ffff; border-radius: 4px;");  // 设置样式：青色边框、圆角
    mapLabel->setAlignment(Qt::AlignCenter);       // 设置内容居中对齐
    mapLabel->setMinimumSize(300, 220);            // 设置最小尺寸300x220像素
    
    // ==================== 将控件添加到布局 ====================
    layout->addWidget(coordLabel);                 // 添加坐标标签到布局
    layout->addWidget(mapLabel);                   // 添加地图标签到布局
    
    // ==================== 创建网络访问管理器 ====================
    networkManager = new QNetworkAccessManager(this);  // 创建网络访问管理器，用于加载地图瓦片
    
    // ==================== 初始化成员变量 ====================
    currentLon = 104.0668;        // 默认经度（成都）
    currentLat = 30.5728;         // 默认纬度（成都）
    destLon = 0.0;                // 目的地经度初始化为0
    destLat = 0.0;                // 目的地纬度初始化为0
    mapLoading = false;           // 地图加载状态初始化为未加载
    currentRequestId = 0;         // 请求ID初始化为0
    currentScale = 1.0;           // 缩放比例初始化为1.0（原始大小）
    isDragging = false;           // 拖动状态初始化为未拖动
    mapOffset = QPoint(0, 0);     // 偏移量初始化为0
    markerX = 0;                  // 标记点X坐标初始化为0
    markerY = 0;                  // 标记点Y坐标初始化为0
}

/**
 * @brief MiniMapDialog类析构函数
 * @details 默认析构函数，Qt会自动管理子控件和网络管理器的内存
 */
MiniMapDialog::~MiniMapDialog()
{
}

/**
 * @brief 将经纬度转换为瓦片坐标
 * @details 使用标准的Web Mercator投影公式将经纬度转换为瓦片坐标
 * @param lon 经度（范围-180到180）
 * @param lat 纬度（范围-85.0511到85.0511）
 * @param zoom 缩放级别（0-18）
 * @param tileX 输出参数，瓦片X坐标
 * @param tileY 输出参数，瓦片Y坐标
 */
void MiniMapDialog::latLngToTile(double lon, double lat, int zoom, int &tileX, int &tileY)
{
    double n = pow(2.0, zoom);                                                                 // 计算2的zoom次方，得到该级别下的瓦片数量
    tileX = (int)floor((lon + 180.0) / 360.0 * n);                                            // 将经度转换为瓦片X坐标
    tileY = (int)floor((1.0 - log(tan(lat * M_PI / 180.0) + 1.0 / cos(lat * M_PI / 180.0)) / M_PI) / 2.0 * n);  // 将纬度转换为瓦片Y坐标
}

/**
 * @brief 加载地图图片
 * @details 通过高德地图瓦片API加载3x3的地图瓦片，合并成一张完整图片，
 *          并在图片上绘制目的地标记点
 * @param lon 中心点经度
 * @param lat 中心点纬度
 */
void MiniMapDialog::loadMapImage(double lon, double lat)
{
    if (mapLoading) {
        return;  // 如果正在加载中，直接返回，避免重复请求
    }
    mapLoading = true;               // 设置加载状态为true
    currentRequestId++;             // 增加请求ID，用于区分新旧请求
    int requestId = currentRequestId;  // 保存当前请求ID到局部变量
    
    // ==================== 配置瓦片加载参数 ====================
    int zoom = 14;                  // 缩放级别14（城市级别）
    int tileSize = 256;             // 每个瓦片大小256x256像素
    int tilesX = 3;                 // 横向加载3个瓦片
    int tilesY = 3;                 // 纵向加载3个瓦片
    
    // ==================== 计算中心点瓦片坐标 ====================
    int centerTileX, centerTileY;
    latLngToTile(lon, lat, zoom, centerTileX, centerTileY);
    
    // ==================== 创建合并图片 ====================
    QImage *mergedImage = new QImage(tileSize * tilesX, tileSize * tilesY, QImage::Format_ARGB32);  // 创建3x3瓦片大小的合并图片
    mergedImage->fill(Qt::black);   // 用黑色填充背景
    
    // ==================== 创建加载计数器 ====================
    int *loadedTiles = new int(0);  // 已加载瓦片计数，使用指针以便在lambda中修改
    int totalTiles = tilesX * tilesY;  // 总瓦片数9个
    
    // ==================== 遍历3x3网格加载所有瓦片 ====================
    for (int dx = -1; dx <= 1; dx++) {      // 横向偏移：-1, 0, 1
        for (int dy = -1; dy <= 1; dy++) {  // 纵向偏移：-1, 0, 1
            int tileX = centerTileX + dx;   // 当前瓦片X坐标
            int tileY = centerTileY + dy;   // 当前瓦片Y坐标
            
            // 计算子域名（高德地图有4个子域名：webst01-webst04）
            int subdomain = (tileX + tileY) % 4 + 1;
            // 构建瓦片URL
            QString urlStr = QString("http://webst0%1.is.autonavi.com/appmaptile?style=6&x=%2&y=%3&z=%4")
                             .arg(subdomain)   // 子域名编号
                             .arg(tileX)       // 瓦片X坐标
                             .arg(tileY)       // 瓦片Y坐标
                             .arg(zoom);       // 缩放级别
            
            QUrl url(urlStr);                 // 创建QUrl对象
            QNetworkRequest request(url);     // 创建网络请求
            
            // 发送GET请求并连接finished信号
            QNetworkReply *reply = networkManager->get(request);
            connect(reply, &QNetworkReply::finished, this, [=]() {
                // 检查是否为最新请求，否则忽略
                if (requestId != currentRequestId) {
                    reply->deleteLater();    // 释放旧请求资源
                    return;
                }
                
                (*loadedTiles)++;            // 增加已加载瓦片计数
                
                // 如果请求成功
                if (reply->error() == QNetworkReply::NoError) {
                    QByteArray data = reply->readAll();  // 读取响应数据
                    QImage tileImage;                    // 创建瓦片图片对象
                    if (tileImage.loadFromData(data)) {  // 从数据加载图片
                        int x = (dx + 1) * tileSize;     // 计算瓦片在合并图片中的X位置
                        int y = (dy + 1) * tileSize;     // 计算瓦片在合并图片中的Y位置
                        
                        QPainter painter(mergedImage);   // 创建绘制器
                        painter.drawImage(x, y, tileImage);  // 将瓦片绘制到合并图片
                        painter.end();                   // 结束绘制
                    }
                }
                
                reply->deleteLater();        // 释放响应对象
                
                // 当所有瓦片加载完成后
                if (*loadedTiles >= totalTiles) {
                    // 创建绘制器，在合并后的图片上绘制标记点
                    QPainter painter(mergedImage);
                    painter.setRenderHint(QPainter::Antialiasing, true);  // 启用抗锯齿
                    
                    // 将经纬度转换为图片上的像素坐标
                    double n = pow(2.0, zoom);
                    double tileXf = (lon + 180.0) / 360.0 * n;
                    double tileYf = (1.0 - log(tan(lat * M_PI / 180.0) + 1.0 / cos(lat * M_PI / 180.0)) / M_PI) / 2.0 * n;
                    
                    // 计算像素坐标（减去左上角瓦片坐标）
                    double pixelX = (tileXf - (centerTileX - 1)) * tileSize;
                    double pixelY = (tileYf - (centerTileY - 1)) * tileSize;
                    
                    // 保存标记点的实际像素坐标，用于居中计算
                    markerX = pixelX;
                    markerY = pixelY;
                    
                    // 绘制蓝色位置标记（外圈白色边框，内圈蓝色填充）
                    painter.setPen(QPen(Qt::white, 2));  // 白色边框，宽度2
                    painter.setBrush(Qt::blue);          // 蓝色填充
                    painter.drawEllipse(QPoint((int)pixelX, (int)pixelY), 8, 8);  // 绘制8x8像素的椭圆
                    
                    // 在标记上方绘制"目的地"文字
                    painter.setPen(Qt::white);           // 白色文字
                    painter.setFont(QFont("Arial", 9, QFont::Bold));  // Arial字体，9号，加粗
                    painter.drawText(QPoint((int)pixelX - 25, (int)pixelY - 12), "目的地");  // 在标记上方绘制文字
                    
                    painter.end();                       // 结束绘制
                    
                    // 将QImage转换为QPixmap
                    QPixmap pixmap = QPixmap::fromImage(*mergedImage);
                    originalPixmap = pixmap;             // 保存原始图片
                    currentScale = 1.0;                  // 重置缩放比例
                    mapOffset = QPoint(0, 0);            // 重置偏移量
                    updateMapDisplay();                  // 更新地图显示
                    
                    // 更新坐标标签显示
                    coordLabel->setText(QString("经度: %1  纬度: %2  缩放: %3%")
                                        .arg(lon, 0, 'f', 6)      // 经度，保留6位小数
                                        .arg(lat, 0, 'f', 6)      // 纬度，保留6位小数
                                        .arg((int)(currentScale * 100)));  // 缩放百分比
                    
                    mapLoading = false;                  // 重置加载状态
                    delete mergedImage;                  // 释放合并图片资源
                    delete loadedTiles;                  // 释放计数器资源
                }
            });
        }
    }
}

/**
 * @brief 设置地图位置
 * @details 设置当前经纬度并加载地图
 * @param lon 经度
 * @param lat 纬度
 */
void MiniMapDialog::setPosition(double lon, double lat)
{
    currentLon = lon;              // 更新当前经度
    currentLat = lat;              // 更新当前纬度
    loadMapImage(lon, lat);        // 加载该位置的地图
}

/**
 * @brief 更新地图位置
 * @details 更新当前经纬度并立即重新加载地图（中断之前的加载）
 * @param lon 新的经度
 * @param lat 新的纬度
 */
void MiniMapDialog::updatePosition(double lon, double lat)
{
    currentLon = lon;              // 更新当前经度
    currentLat = lat;              // 更新当前纬度
    
    // 重置加载标志，允许立即加载新位置（中断之前的加载）
    mapLoading = false;
    loadMapImage(lon, lat);        // 加载新位置的地图
    
    qDebug() << "[小地图] 更新位置到目的地:" << lon << "," << lat;  // 调试输出
}

/**
 * @brief 地图图片加载完成槽函数（备用）
 * @details 处理地图图片加载完成后的逻辑，当前主要由loadMapImage中的lambda处理
 * @param reply 网络响应对象
 */
void MiniMapDialog::onMapImageLoaded(QNetworkReply *reply)
{
    reply->deleteLater();          // 释放响应对象
}

/**
 * @brief 更新地图显示
 * @details 根据当前缩放比例和偏移量更新地图显示，确保标记点始终居中
 */
void MiniMapDialog::updateMapDisplay()
{
    if (originalPixmap.isNull()) {
        return;  // 如果原始图片为空，直接返回
    }
    
    // 计算缩放后的图片大小
    QSize scaledSize = originalPixmap.size() * currentScale;
    // 缩放图片（保持宽高比，平滑变换）
    QPixmap scaledPixmap = originalPixmap.scaled(scaledSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    
    // 创建一个新的显示图片，大小与mapLabel一致
    QPixmap displayPixmap(mapLabel->size());
    displayPixmap.fill(Qt::transparent);  // 用透明填充
    
    // 计算基础偏移量：让标记点(markerX, markerY)对应到mapLabel中心
    int markerScaledX = markerX * currentScale;   // 标记点缩放后的X坐标
    int markerScaledY = markerY * currentScale;   // 标记点缩放后的Y坐标
    int labelCenterX = mapLabel->width() / 2;     // mapLabel中心X坐标
    int labelCenterY = mapLabel->height() / 2;    // mapLabel中心Y坐标
    
    // 计算基础偏移量，使标记点位于mapLabel中心
    int baseOffsetX = labelCenterX - markerScaledX;
    int baseOffsetY = labelCenterY - markerScaledY;
    
    // 应用用户拖动的偏移量
    int finalOffsetX = baseOffsetX + mapOffset.x();
    int finalOffsetY = baseOffsetY + mapOffset.y();
    
    // 将缩放后的图片绘制到显示图片上
    QPainter painter(&displayPixmap);
    painter.drawPixmap(QPoint(finalOffsetX, finalOffsetY), scaledPixmap);
    painter.end();
    
    // 更新mapLabel显示
    mapLabel->setPixmap(displayPixmap);
}

/**
 * @brief 重写鼠标滚轮事件
 * @details 处理鼠标滚轮事件，实现地图缩放功能（向前放大，向后缩小）
 * @param event 滚轮事件对象
 */
void MiniMapDialog::wheelEvent(QWheelEvent *event)
{
    if (originalPixmap.isNull()) {
        return;  // 如果原始图片为空，直接返回
    }
    
    int delta = event->delta();    // 获取滚轮增量（正数向前，负数向后）
    double scaleFactor = 1.1;      // 缩放因子（每次缩放10%）
    
    if (delta > 0) {
        // 滚轮向前，放大地图
        currentScale *= scaleFactor;
    } else {
        // 滚轮向后，缩小地图
        currentScale /= scaleFactor;
    }
    
    // 限制缩放范围：最小0.5倍，最大3.0倍
    currentScale = qMax(0.5, qMin(currentScale, 3.0));
    
    updateMapDisplay();            // 更新地图显示
    
    // 更新坐标标签显示
    coordLabel->setText(QString("经度: %1  纬度: %2  缩放: %3%")
                        .arg(currentLon, 0, 'f', 6)
                        .arg(currentLat, 0, 'f', 6)
                        .arg((int)(currentScale * 100)));
    
    event->accept();               // 接受事件，阻止传递给父控件
}

/**
 * @brief 重写鼠标按下事件
 * @details 处理鼠标按下事件，开始地图拖动（左键按下且在mapLabel范围内）
 * @param event 鼠标事件对象
 */
void MiniMapDialog::mousePressEvent(QMouseEvent *event)
{
    // 判断是否在mapLabel范围内按下左键
    if (event->button() == Qt::LeftButton && mapLabel->geometry().contains(event->pos())) {
        isDragging = true;         // 设置拖动状态为true
        lastMousePos = event->pos();  // 记录当前鼠标位置
        mapLabel->setCursor(Qt::ClosedHandCursor);  // 改变鼠标样式为闭合手形
    }
    QDialog::mousePressEvent(event);  // 调用父类事件处理
}

/**
 * @brief 重写鼠标移动事件
 * @details 处理鼠标移动事件，实现地图拖动功能
 * @param event 鼠标事件对象
 */
void MiniMapDialog::mouseMoveEvent(QMouseEvent *event)
{
    if (isDragging) {              // 如果正在拖动
        QPoint delta = event->pos() - lastMousePos;  // 计算鼠标移动增量
        mapOffset += delta;        // 更新地图偏移量
        
        // 限制偏移范围，确保地图不会完全移出可视区域
        QSize scaledSize = originalPixmap.size() * currentScale;  // 缩放后的地图大小
        int maxOffsetX = qMax(0, scaledSize.width() - mapLabel->width());   // 最大X偏移
        int maxOffsetY = qMax(0, scaledSize.height() - mapLabel->height()); // 最大Y偏移
        
        // 限制偏移量在[-maxOffset, maxOffset]范围内
        mapOffset.setX(qBound(-maxOffsetX, mapOffset.x(), maxOffsetX));
        mapOffset.setY(qBound(-maxOffsetY, mapOffset.y(), maxOffsetY));
        
        updateMapDisplay();        // 更新地图显示
        lastMousePos = event->pos();  // 更新上一次鼠标位置
    }
    QDialog::mouseMoveEvent(event);  // 调用父类事件处理
}

/**
 * @brief 重写鼠标释放事件
 * @details 处理鼠标释放事件，结束地图拖动
 * @param event 鼠标事件对象
 */
void MiniMapDialog::mouseReleaseEvent(QMouseEvent *event)
{
    // 如果释放的是左键且正在拖动
    if (event->button() == Qt::LeftButton && isDragging) {
        isDragging = false;        // 设置拖动状态为false
        mapLabel->setCursor(Qt::OpenHandCursor);  // 恢复鼠标样式为张开手形
    }
    QDialog::mouseReleaseEvent(event);  // 调用父类事件处理
}

/**
 * @brief 设置目的地坐标
 * @details 设置目的地经纬度，用于双击还原时显示目的地位置
 * @param lon 目的地经度
 * @param lat 目的地纬度
 */
void MiniMapDialog::setDestination(double lon, double lat)
{
    destLon = lon;                 // 更新目的地经度
    destLat = lat;                 // 更新目的地纬度
}

/**
 * @brief 重写鼠标双击事件
 * @details 处理鼠标双击事件，还原地图到初始状态（重置缩放和偏移）
 * @param event 鼠标事件对象
 */
void MiniMapDialog::mouseDoubleClickEvent(QMouseEvent *event)
{
    // 判断是否在mapLabel范围内双击左键
    if (event->button() == Qt::LeftButton && mapLabel->geometry().contains(event->pos())) {
        // 双击还原：重置缩放比例和偏移量
        currentScale = 1.0;
        mapOffset = QPoint(0, 0);
        
        // 如果有目的地坐标，重新加载地图并居中显示目的地
        if (destLon != 0.0 && destLat != 0.0) {
            loadMapImage(destLon, destLat);
        } else {
            // 如果没有目的地，重新加载当前位置
            loadMapImage(currentLon, currentLat);
        }
    }
    QDialog::mouseDoubleClickEvent(event);  // 调用父类事件处理
}