/**
 * @file widget.cpp
 * @brief 飞行数据界面主窗口类实现文件
 * @details 该文件实现了Widget类的所有成员函数，包括构造函数、UI初始化和样式设置。
 *          界面采用纯代码方式创建，不依赖Qt Designer生成的.ui文件。
 */

// 包含Widget类的头文件
#include "widget.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QCoreApplication>
#include <QFile>
#include <QDebug>
#include <cmath>
#include <QPainter>
#include <QSslConfiguration>
#include <QSslSocket>
#include <QStyle>
#include <QButtonGroup>
#include <QSettings>
#include <opencv2/opencv.hpp>   // OpenCV：USB 摄像头视频采集（降落时启动，显示到视频显示窗口）

/**
 * @brief 校验经纬度坐标是否有效（文件级辅助函数）
 * @param lon 经度
 * @param lat 纬度
 * @return true=有效坐标，false=无效坐标
 * @details 校验规则：非NaN、非Inf、经度[-180,180]、纬度[-90,90]、(0,0)视为未初始化
 */
static bool isCoordValid(double lon, double lat)
{
    if (qIsNaN(lon) || qIsNaN(lat)) return false;   // 检查是否为非数字
    if (qIsInf(lon) || qIsInf(lat)) return false;   // 检查是否为无穷大
    if (lon < -180.0 || lon > 180.0) return false;  // 经度范围检查
    if (lat <  -90.0 || lat >  90.0) return false;  // 纬度范围检查
    if (lon == 0.0 && lat == 0.0) return false;     // (0,0) 视为未初始化
    return true;
}

/**
 * @brief 构建快捷键默认状态的内联样式字符串
 * @details 使用纯内联样式（setStyleSheet），不使用任何伪状态选择器
 *          Qt的setStyleSheet只支持简单属性，不支持:hover/:pressed等伪状态子选择器
 *          所有按钮样式完全由内联样式控制，QSS中不再包含这些按钮的任何规则
 * @return 默认样式字符串，可直接传给 setStyleSheet()
 */
static QString buildShortcutBtnDefaultStyle()
{
    return "padding: 5px 12px;"                                           // 内边距
           "border-radius: 4px;"                                          // 圆角
           "border: 1px solid rgba(0, 212, 255, 0.55);"                   // 青蓝边框
           "border-top: 1px solid rgba(140, 215, 245, 0.85);"             // 3D顶部高光
           "border-left: 1px solid rgba(110, 200, 235, 0.8);"             // 3D左侧高光
           "border-right: 1px solid rgba(0, 80, 110, 0.85);"              // 3D右侧阴影
           "border-bottom: 1px solid rgba(0, 55, 80, 0.9);"              // 3D底部阴影
           "background: qlineargradient(x1:0, y1:0, x2:0, y2:1, "
           "stop:0 rgba(56, 75, 98, 0.95), "
           "stop:0.5 rgba(30, 40, 52, 0.92), "
           "stop:1 rgba(8, 12, 18, 0.98));"
           "color: #00d4ff;"                                              // 青蓝文字
           "font-size: 11px;"                                             // 字体大小
           "font-weight: bold;";                                          // 字体加粗
}

/**
 * @brief 构建快捷键米色选中状态的内联样式字符串
 * @details 与默认样式使用完全相同的尺寸属性（padding/border-width/font-size），
 *          仅改变颜色/背景，确保切换时按钮尺寸不发生变化
 * @return 米色选中样式字符串，可直接传给 setStyleSheet()
 */
static QString buildShortcutBtnBeigeStyle()
{
    return "padding: 5px 12px;"                                           // 内边距：与默认样式一致
           "border-radius: 4px;"                                          // 圆角：与默认样式一致
           "border: 1px solid #ffd600;"                                    // 警示黄边框（宽度1px，与默认一致）
           "border-top: 1px solid #fff066;"                               // 3D顶部高光
           "border-left: 1px solid #ffe633;"                              // 3D左侧高光
           "border-right: 1px solid #aa8c00;"                             // 3D右侧阴影
           "border-bottom: 1px solid #886b00;"                            // 3D底部阴影
           "background: qlineargradient(x1:0, y1:0, x2:0, y2:1, "
           "stop:0 rgba(255, 224, 26, 0.4), "                             // 顶部：亮黄
           "stop:0.5 rgba(140, 110, 10, 0.88), "                          // 中部：深黄棕
           "stop:1 rgba(60, 48, 4, 0.98));"                               // 底部：暗黄棕
           "color: #ffe34d;"                                              // 亮黄文字（高对比）
           "font-size: 11px;"                                             // 字体大小：与默认一致
           "font-weight: bold;";                                          // 字体加粗：与默认一致
}

/**
 * @brief 重置按钮为默认内联样式
 * @details 将指定按钮的内联样式设置为默认样式，确保与选中状态使用相同的渲染路径
 * @param btn 需要恢复默认样式的按钮指针
 */
static void applyDefaultStyle(QPushButton* btn)
{
    btn->setStyleSheet(buildShortcutBtnDefaultStyle());                   // 应用默认内联样式
    btn->style()->unpolish(btn);                                          // 取消旧样式
    btn->style()->polish(btn);                                            // 应用新样式
}

/**
 * @brief 重置分组内所有按钮为默认内联样式
 * @details 遍历分组内所有按钮，逐一应用默认内联样式
 * @param group 按钮分组（高度组或速度组）
 */
static void applyDefaultStyleToGroup(QButtonGroup* group)
{
    QList<QAbstractButton*> buttons = group->buttons();                   // 获取分组内所有按钮
    for (QAbstractButton* b : buttons) {
        QPushButton* pb = qobject_cast<QPushButton*>(b);                 // 转换为QPushButton
        if (pb) {
            applyDefaultStyle(pb);                                        // 应用默认内联样式
        }
    }
}

/**
 * @brief Widget类构造函数
 * @details 初始化窗口标题和大小，调用setupUI()和applyStyles()完成界面构建和样式设置
 * @param parent 父窗口指针，默认为nullptr
 */
Widget::Widget(QWidget *parent)  // 构造函数定义，接收父窗口指针参数
    : QWidget(parent)            // 调用父类QWidget的构造函数，传递父窗口指针
{
    setWindowTitle("飞行数据");  // 设置窗口标题为"飞行数据"
    setFixedSize(890, 640);       // 设置窗口固定大小，禁止拖拽调整，修改此参数即可控制窗口尺寸
    setAttribute(Qt::WA_StyledBackground, true);  // 启用样式表背景绘制，确保QSS背景色生效
    
    // ==================== 初始化飞行状态变量 ====================
    isPowerOn = false;           // 电源初始状态为关闭（false表示关机状态）
    isFlying = false;            // 飞行初始状态为未起飞（false表示地面状态）
    isHovering = false;          // 悬停初始状态为未悬停（false表示正常飞行状态）
    isLanding = false;           // 降落初始状态为未降落（false表示不在降落过程中）
    hoverMode = FORWARD_HOVER;   // 悬停模式默认为前进悬停
    hoverBackwardSeconds = 0;    // 后退悬停计时器初始化为0
    hoverHeading = 0.0;          // 盘旋悬停航向角初始化为0度
    positionWebView = nullptr;     // 位置信息框动态地图视图初始化为空指针
    positionWebPage = nullptr;     // 位置信息框动态地图页面初始化为空指针
    positionMapPlaceholder = nullptr; // 布局占位控件初始化为空指针
    positionFramePtr = nullptr;       // 位置信息框框架指针初始化为空指针
    mapIconLabel = nullptr;             // 地图图标标签初始化为空指针
    mapTitleLabel = nullptr;            // 地图标题标签初始化为空指针
    videoDisplayLabel = nullptr;      // 视频显示窗口标签初始化为空指针
    videoCapture = nullptr;          // OpenCV 视频采集对象初始化为空指针（降落时才打开摄像头）
    videoTimer = nullptr;            // 视频帧采集定时器初始化为空指针
    positionMapReady = false;      // 位置信息框动态地图未就绪
    positionMapLoaded = false;     // 位置信息框动态地图页面尚未加载（延迟到首次显示后）
    positionRetryCount = 0;        // 地图就绪检查重试计数初始化为0
    lastPosSentLon = 0;            // 上次发送经度初始化为0
    lastPosSentLat = 0;            // 上次发送纬度初始化为0
    
    // ==================== 初始化弹窗对话框指针 ====================
    compassWidget = nullptr;      // 指南针控件初始化为空指针
    mapDialog = nullptr;         // 地图弹窗初始化为空指针
    miniMapDialog = nullptr;     // 小地图弹窗初始化为空指针
    m_positionMapWasVisible = false;  // 位置地图在弹窗打开前的可见性
    hoverModeDialog = nullptr;   // 悬停模式对话框初始化为空指针
    btnHoverCircle = nullptr;   // 悬停对话框盘旋按钮初始化为空指针
    btnHoverStop = nullptr;     // 悬停对话框悬停按钮初始化为空指针
    btnHoverBack = nullptr;     // 悬停对话框后退按钮初始化为空指针
    rotationDial = nullptr;     // 旋转圆盘控件初始化为空指针
    rotationDialDialog = nullptr;  // 旋转圆盘对话框初始化为空指针
    rotationTargetAngle = 0;    // 目标旋转角度初始化为0
    rotationCurrentAngle = 0;   // 当前已旋转角度初始化为0
    isRotating = false;         // 旋转状态初始化为未旋转
    
    // ==================== 初始化飞行数据变量 ====================
    flightHeight = 0.0;          // 初始飞行高度为0米（地面状态）
    targetFlightHeight = 10.0;   // 初始目标飞行高度为10米
    flightSpeed = 0.0;           // 初始飞行速度为0 km/h（静止状态）
    maxFlightSpeed = 10.0;       // 初始最大速度为10 km/h
    batteryLevel = 0;            // 初始电池电量为0%（关机时无电量显示）
    for (int i = 0; i < 6; i++) {
        batteryPercentages[i] = 0;
        batteryInstalled[i] = false;  // 初始状态：所有电池均未安装
    }
    currentBatteryIndex = 0;
    lowBatteryWarned = false;
    flightWeight = 0.0;          // 初始重量为0 kg（关机时无重量显示）
    latitude = 0.0;              // 初始纬度为0度（未定位）
    longitude = 0.0;             // 初始经度为0度（未定位）
    takeoffLatitude = 0.0;       // 初始起飞点纬度为0度
    takeoffLongitude = 0.0;      // 初始起飞点经度为0度
    flightSeconds = 0;           // 初始当前飞行时长为0秒
    totalFlightSeconds = 0;      // 初始总飞行时长为0秒（开机时从QSettings加载）
    destinationDistance = 6.80;  // 初始目的地距离为6.80公里
    totalDistance = 0.0;        // 初始总行驶距离为0公里（开机时从QSettings加载累计值）
    traveledDistance = 0.0;      // 初始已行驶距离为0公里
    
    // ==================== 初始化灯光状态 ====================
    lightsState = 0;             // 灯光状态初始化为0：0=关闭(灯光), 1=近光, 2=远光
    
    // ==================== 初始化灯光样式表字符串 ====================
    // 灯光关闭时的样式：磨砂深灰椭圆 + 青蓝描边（立体浮雕）
    lightsOffStyle = "width: 65px; height: 45px; border-radius: 22px; "
                     "border: 1.5px solid rgba(0, 212, 255, 0.7); border-top: 1.5px solid rgba(140, 215, 245, 0.9); border-left: 1.5px solid rgba(110, 200, 235, 0.85); border-right: 1.5px solid rgba(0, 80, 110, 0.9); border-bottom: 1.5px solid rgba(0, 55, 80, 0.95);"
                     "background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 rgba(44, 58, 74, 0.9), stop:1 rgba(18, 24, 32, 0.95)); "
                     "color: #00d4ff; font-size: 13px; font-weight: bold; text-align: center; "
                     "box-shadow: inset 0 1px 2px rgba(0, 212, 255, 0.22), inset 0 -2px 3px rgba(0,0,0,0.45), 0 0 8px rgba(0, 212, 255, 0.12), 0 3px 8px rgba(0,0,0,0.6);";
    lightsLowStyle = "width: 65px; height: 45px; border-radius: 22px; "
                     "border: 1.5px solid #00e676; border-top: 1.5px solid #66ffaa; border-left: 1.5px solid #4dffa0; border-right: 1.5px solid #008040; border-bottom: 1.5px solid #006030;"
                     "background: qradialgradient(cx:0.35, cy:0.3, radius:0.85, fx:0.35, fy:0.3, stop:0 rgba(0, 230, 118, 0.45), stop:0.6 rgba(0, 90, 50, 0.85), stop:1 rgba(0, 40, 22, 0.95)); "
                     "color: #00e676; font-size: 13px; font-weight: bold; text-align: center; "
                     "box-shadow: inset 0 1px 2px rgba(255,255,255,0.25), 0 0 16px rgba(0, 230, 118, 0.4), 0 3px 8px rgba(0,120,60,0.6);";
    lightsHighStyle = "width: 65px; height: 45px; border-radius: 22px; "
                       "border: 1.5px solid #ffd600; border-top: 1.5px solid #fff066; border-left: 1.5px solid #ffe633; border-right: 1.5px solid #aa8c00; border-bottom: 1.5px solid #886b00;"
                       "background: qradialgradient(cx:0.35, cy:0.3, radius:0.85, fx:0.35, fy:0.3, stop:0 rgba(255, 214, 0, 0.5), stop:0.6 rgba(150, 120, 10, 0.85), stop:1 rgba(80, 60, 5, 0.95)); "
                       "color: #ffffff; font-size: 13px; font-weight: bold; text-align: center; "
                       "box-shadow: inset 0 1px 2px rgba(255,255,255,0.3), 0 0 20px rgba(255, 214, 0, 0.5), 0 3px 8px rgba(200,150,0,0.6);";
    lightsOnStyle = "width: 65px; height: 45px; border-radius: 22px; "
                    "border: 1.5px solid #00e676; border-top: 1.5px solid #66ffaa; border-left: 1.5px solid #4dffa0; border-right: 1.5px solid #008040; border-bottom: 1.5px solid #006030;"
                    "background: qradialgradient(cx:0.35, cy:0.3, radius:0.85, fx:0.35, fy:0.3, stop:0 rgba(0, 230, 118, 0.45), stop:0.6 rgba(0, 100, 50, 0.85), stop:1 rgba(0, 45, 20, 0.95)); "
                    "color: #00e676; font-size: 13px; font-weight: bold; text-align: center; "
                    "box-shadow: inset 0 1px 2px rgba(255,255,255,0.25), 0 0 16px rgba(0, 230, 118, 0.4), 0 3px 8px rgba(0,140,70,0.6);";

    // 近光按钮样式（小椭圆形，关闭状态）- 磨砂深灰 + 青蓝描边
    lowLightOffStyle = "width: 50px; height: 35px; border-radius: 17px; "
                       "border: 1.5px solid rgba(0, 212, 255, 0.65); border-top: 1.5px solid rgba(140, 215, 245, 0.85); border-left: 1.5px solid rgba(110, 200, 235, 0.8); border-right: 1.5px solid rgba(0, 80, 110, 0.85); border-bottom: 1.5px solid rgba(0, 55, 80, 0.9);"
                       "background: qradialgradient(cx:0.35, cy:0.3, radius:0.85, fx:0.35, fy:0.3, stop:0 rgba(40, 54, 70, 0.88), stop:1 rgba(16, 22, 30, 0.94)); "
                       "color: #00d4ff; font-size: 11px; font-weight: bold; text-align: center; "
                       "box-shadow: inset 0 1px 2px rgba(0, 212, 255, 0.2), inset 0 -2px 3px rgba(0,0,0,0.4), 0 0 6px rgba(0, 212, 255, 0.1), 0 2px 6px rgba(0,0,0,0.55);";

    // 近光按钮样式（小椭圆形，开启状态）- 绿色状态色
    lowLightOnStyle = "width: 50px; height: 35px; border-radius: 17px; "
                      "border: 1.5px solid #00e676; border-top: 1.5px solid #66ffaa; border-left: 1.5px solid #4dffa0; border-right: 1.5px solid #008040; border-bottom: 1.5px solid #006030;"
                      "background: qradialgradient(cx:0.35, cy:0.3, radius:0.85, fx:0.35, fy:0.3, stop:0 rgba(0, 230, 118, 0.55), stop:0.6 rgba(0, 90, 45, 0.88), stop:1 rgba(0, 40, 18, 0.95)); "
                      "color: #00e676; font-size: 11px; font-weight: bold; text-align: center; "
                      "box-shadow: inset 0 1px 2px rgba(255,255,255,0.25), 0 0 14px rgba(0, 230, 118, 0.4), 0 2px 6px rgba(0,140,70,0.55);";

    // 远光按钮样式（小椭圆形，关闭状态）- 磨砂深灰 + 青蓝描边
    highLightOffStyle = "width: 50px; height: 35px; border-radius: 17px; "
                        "border: 1.5px solid rgba(0, 212, 255, 0.65); border-top: 1.5px solid rgba(140, 215, 245, 0.85); border-left: 1.5px solid rgba(110, 200, 235, 0.8); border-right: 1.5px solid rgba(0, 80, 110, 0.85); border-bottom: 1.5px solid rgba(0, 55, 80, 0.9);"
                        "background: qradialgradient(cx:0.35, cy:0.3, radius:0.85, fx:0.35, fy:0.3, stop:0 rgba(40, 54, 70, 0.88), stop:1 rgba(16, 22, 30, 0.94)); "
                        "color: #00d4ff; font-size: 11px; font-weight: bold; text-align: center; "
                        "box-shadow: inset 0 1px 2px rgba(0, 212, 255, 0.2), inset 0 -2px 3px rgba(0,0,0,0.4), 0 0 6px rgba(0, 212, 255, 0.1), 0 2px 6px rgba(0,0,0,0.55);";

    // 远光按钮样式（小椭圆形，开启状态）- 黄色警示色，强发光
    highLightOnStyle = "width: 50px; height: 35px; border-radius: 17px; "
                       "border: 1.5px solid #ffd600; border-top: 1.5px solid #fff066; border-left: 1.5px solid #ffe633; border-right: 1.5px solid #aa8c00; border-bottom: 1.5px solid #886b00;"
                       "background: qradialgradient(cx:0.35, cy:0.3, radius:0.85, fx:0.35, fy:0.3, stop:0 rgba(255, 214, 0, 0.6), stop:0.6 rgba(180, 150, 10, 0.88), stop:1 rgba(90, 70, 5, 0.95)); "
                       "color: #ffffff; font-size: 11px; font-weight: bold; text-align: center; "
                       "box-shadow: inset 0 1px 2px rgba(255,255,255,0.35), 0 0 22px rgba(255, 214, 0, 0.55), 0 2px 6px rgba(200,150,0,0.6);";

    lightsLowStyle = "width: 65px; height: 45px; border-radius: 22px; "
                     "border: 1.5px solid #00e676; border-top: 1.5px solid #66ffaa; border-left: 1.5px solid #4dffa0; border-right: 1.5px solid #008040; border-bottom: 1.5px solid #006030;"
                     "background: qradialgradient(cx:0.35, cy:0.3, radius:0.85, fx:0.35, fy:0.3, stop:0 rgba(0, 230, 118, 0.45), stop:1 rgba(0, 80, 40, 0.85)); "
                     "color: #00e676; font-size: 13px; font-weight: bold; text-align: center; "
                     "box-shadow: inset 0 1px 2px rgba(255,255,255,0.25), 0 3px 8px rgba(0,150,70,0.5);";
    lightsHighStyle = "width: 65px; height: 45px; border-radius: 22px; "
                       "border: 1.5px solid #00e676; border-top: 1.5px solid #66ffaa; border-left: 1.5px solid #4dffa0; border-right: 1.5px solid #008040; border-bottom: 1.5px solid #006030;"
                       "background: qradialgradient(cx:0.35, cy:0.3, radius:0.85, fx:0.35, fy:0.3, stop:0 rgba(0, 230, 118, 0.55), stop:1 rgba(0, 100, 45, 0.9)); "
                       "color: #00e676; font-size: 13px; font-weight: bold; text-align: center; "
                       "box-shadow: inset 0 1px 2px rgba(255,255,255,0.3), 0 3px 8px rgba(0,200,90,0.55);";
    lightsOnStyle = "width: 65px; height: 45px; border-radius: 22px; "
                    "border: 1.5px solid #00e676; border-top: 1.5px solid #66ffaa; border-left: 1.5px solid #4dffa0; border-right: 1.5px solid #008040; border-bottom: 1.5px solid #006030;"
                    "background: qradialgradient(cx:0.35, cy:0.3, radius:0.85, fx:0.35, fy:0.3, stop:0 rgba(0, 200, 100, 0.45), stop:1 rgba(0, 60, 30, 0.85)); "
                    "color: #00e676; font-size: 13px; font-weight: bold; text-align: center; "
                    "box-shadow: inset 0 1px 2px rgba(255,255,255,0.25), 0 3px 8px rgba(0,150,70,0.5);";

    // 近光按钮样式（小椭圆形，关闭状态）- 磨砂深灰 + 青蓝描边
    lowLightOffStyle = "width: 50px; height: 35px; border-radius: 17px; "
                       "border: 1.5px solid rgba(0, 212, 255, 0.65); border-top: 1.5px solid rgba(140, 215, 245, 0.85); border-left: 1.5px solid rgba(110, 200, 235, 0.8); border-right: 1.5px solid rgba(0, 80, 110, 0.85); border-bottom: 1.5px solid rgba(0, 55, 80, 0.9);"
                       "background: qradialgradient(cx:0.35, cy:0.3, radius:0.85, fx:0.35, fy:0.3, stop:0 rgba(40, 54, 70, 0.88), stop:1 rgba(16, 22, 30, 0.94)); "
                       "color: #00d4ff; font-size: 11px; font-weight: bold; text-align: center; "
                       "box-shadow: inset 0 1px 2px rgba(0, 212, 255, 0.2), inset 0 -2px 3px rgba(0,0,0,0.4), 0 2px 6px rgba(0,0,0,0.5);";

    // 近光按钮样式（小椭圆形，开启状态）
    lowLightOnStyle = "width: 50px; height: 35px; border-radius: 17px; "
                      "border: 1.5px solid #00e676; border-top: 1.5px solid #66ffaa; border-left: 1.5px solid #4dffa0; border-right: 1.5px solid #008040; border-bottom: 1.5px solid #006030;"
                      "background: qradialgradient(cx:0.35, cy:0.3, radius:0.85, fx:0.35, fy:0.3, stop:0 rgba(0, 200, 100, 0.55), stop:1 rgba(0, 80, 35, 0.9)); "
                      "color: #00e676; font-size: 11px; font-weight: bold; text-align: center; "
                      "box-shadow: inset 0 1px 2px rgba(255,255,255,0.25), 0 2px 6px rgba(0,150,70,0.5);";

    // 远光按钮样式（小椭圆形，关闭状态）- 磨砂深灰 + 青蓝描边
    highLightOffStyle = "width: 50px; height: 35px; border-radius: 17px; "
                        "border: 1.5px solid rgba(0, 212, 255, 0.65); border-top: 1.5px solid rgba(140, 215, 245, 0.85); border-left: 1.5px solid rgba(110, 200, 235, 0.8); border-right: 1.5px solid rgba(0, 80, 110, 0.85); border-bottom: 1.5px solid rgba(0, 55, 80, 0.9);"
                        "background: qradialgradient(cx:0.35, cy:0.3, radius:0.85, fx:0.35, fy:0.3, stop:0 rgba(40, 54, 70, 0.88), stop:1 rgba(16, 22, 30, 0.94)); "
                        "color: #00d4ff; font-size: 11px; font-weight: bold; text-align: center; "
                        "box-shadow: inset 0 1px 2px rgba(0, 212, 255, 0.2), inset 0 -2px 3px rgba(0,0,0,0.4), 0 2px 6px rgba(0,0,0,0.5);";

    // 远光按钮样式（小椭圆形，开启状态）- 黄色警示色
    highLightOnStyle = "width: 50px; height: 35px; border-radius: 17px; "
                       "border: 1.5px solid #ffd600; border-top: 1.5px solid #fff066; border-left: 1.5px solid #ffe633; border-right: 1.5px solid #aa8c00; border-bottom: 1.5px solid #886b00;"
                       "background: qradialgradient(cx:0.35, cy:0.3, radius:0.85, fx:0.35, fy:0.3, stop:0 rgba(255, 214, 0, 0.6), stop:1 rgba(150, 120, 5, 0.9)); "
                       "color: #ffffff; font-size: 11px; font-weight: bold; text-align: center; "
                       "box-shadow: inset 0 1px 2px rgba(255,255,255,0.3), 0 2px 6px rgba(255,200,0,0.55);";
    
    // ==================== 创建数据更新定时器 ====================
    dataUpdateTimer = new QTimer(this);  // 创建QTimer对象，父对象为当前Widget
    connect(dataUpdateTimer, &QTimer::timeout, this, &Widget::updateFlightData);  // 连接定时器超时信号到数据更新槽函数
    
    // ==================== 创建时间更新定时器 ====================
    timeUpdateTimer = new QTimer(this);  // 创建QTimer对象，父对象为当前Widget
    connect(timeUpdateTimer, &QTimer::timeout, this, &Widget::updateTime);  // 连接定时器超时信号到时间更新槽函数
    
    // ==================== 创建飞行时长定时器 ====================
    flightDurationTimer = new QTimer(this);  // 创建QTimer对象，父对象为当前Widget
    connect(flightDurationTimer, &QTimer::timeout, this, &Widget::updateFlightDuration);  // 连接定时器超时信号到飞行时长更新槽函数
    
    // ==================== 创建地图位置更新定时器 ====================
    mapUpdateTimer = new QTimer(this);  // 创建QTimer对象，父对象为当前Widget
    connect(mapUpdateTimer, &QTimer::timeout, this, &Widget::updateMapPosition);  // 连接定时器超时信号到地图位置更新槽函数
    
    // ==================== 创建目的地搜索防抖定时器 ====================
    destSearchTimer = new QTimer(this);  // 创建QTimer对象，父对象为当前Widget
    destSearchTimer->setSingleShot(true);  // 单次触发模式
    destSearchTimer->setInterval(500);  // 500ms防抖延迟
    connect(destSearchTimer, &QTimer::timeout, this, &Widget::onDestSearchTimeout);  // 连接定时器超时信号到搜索处理槽函数
    
    // ==================== 创建归还按钮状态重置定时器 ====================
    returnResetTimer = new QTimer(this);  // 创建QTimer对象，父对象为当前Widget
    returnResetTimer->setSingleShot(true);  // 单次触发模式
    returnResetTimer->setInterval(5000);  // 5秒后恢复状态
    connect(returnResetTimer, &QTimer::timeout, [this]() {
        // 5秒后恢复结束按钮为默认状态（青蓝立体效果）
        QString defaultStyle = "width: 55px; height: 55px; border-radius: 27px; "
                               "border: 1.5px solid rgba(0, 212, 255, 0.7); border-top: 1.5px solid rgba(140, 215, 245, 0.9); border-left: 1.5px solid rgba(110, 200, 235, 0.85); border-right: 1.5px solid rgba(0, 80, 110, 0.9); border-bottom: 1.5px solid rgba(0, 55, 80, 0.95);"
                               "background: qradialgradient(cx:0.3, cy:0.25, radius:0.9, fx:0.3, fy:0.25, stop:0 rgba(82, 108, 138, 0.95), stop:0.4 rgba(42, 56, 72, 0.93), stop:0.8 rgba(20, 28, 38, 0.97), stop:1 rgba(6, 10, 16, 1)); "
                               "color: #00d4ff; font-size: 12px; font-weight: bold; text-align: center; "
                               "box-shadow: inset 0 1px 2px rgba(0, 212, 255, 0.25), inset 0 -2px 4px rgba(0,0,0,0.5), 0 0 10px rgba(0, 212, 255, 0.15), 0 3px 8px rgba(0,0,0,0.65);";
        btnReturn->setStyleSheet(defaultStyle);
        btnReturn->style()->unpolish(btnReturn);
        btnReturn->style()->polish(btnReturn);
        // 弹窗提示结束成功
        QMessageBox::information(this, "提示", "结束成功");
    });
    
    // ==================== 创建网络访问管理器 ====================
    networkManager = new QNetworkAccessManager(this);  // 创建网络访问管理器，用于获取位置信息
    gpsProcess = nullptr;  // GPS定位进程初始化为空，在requestRealLocation中创建

    // ==================== PX4 仿真控制器初始化 ====================
    px4Connected = false;       // PX4 初始未连接
    px4Armed = false;           // PX4 初始未解锁
    px4WasInAir = false;        // 无人机初始未升空过
    px4FlightMode = Px4Controller::Mode_Unknown;
    px4Controller = nullptr;
    px4PollTimer = nullptr;
    lblPx4Status = nullptr;

    setupUI();                  // 调用setupUI()方法初始化主界面布局
    applyStyles();              // 调用applyStyles()方法应用界面样式

    // 初始化霓虹发光效果（在样式应用后设置）
    setupNeonGlowEffects();

    // 初始化扫描线动画
    m_scanLineY = 0;
    m_scanLineDirDown = true;
    m_scanLineTimer = new QTimer(this);
    m_scanLineTimer->setInterval(33);  // ~30fps
    connect(m_scanLineTimer, &QTimer::timeout, this, &Widget::onScanLineTimer);
    m_scanLineTimer->start();

    // 初始化 PX4 控制器（创建实例、连接信号槽、启动工作线程，但不自动连接 PX4）
    initPx4Controller();

    // 初始化界面数据为0或空（关机状态下所有数据为空）
    refreshUI();

    // 程序启动即开始更新时间显示，无需等待电源开启
    updateTime();                       // 立即刷新一次，显示当前中国时间
    timeUpdateTimer->start(1000);       // 每1秒更新一次时间显示
}

/**
 * @brief Widget类析构函数
 * @details 默认析构函数，Qt会自动管理子控件内存
 */
Widget::~Widget()  // 析构函数定义
{
    // 停止视频显示并释放摄像头资源（确保进程退出时不占用设备）
    stopVideoDisplay();
    if (videoTimer) {
        delete videoTimer;
        videoTimer = nullptr;
    }

    // 停止 PX4 遥测轮询并断开连接
    if (px4PollTimer) {
        px4PollTimer->stop();
        delete px4PollTimer;
        px4PollTimer = nullptr;
    }
    if (px4Controller) {
        px4Controller->disconnect();
        px4Controller->stop();
        // Px4Controller 的 QThread 会在 finished 信号中自动 deleteLater
        // 但 px4Controller 对象本身需要手动释放（它没有 parent）
        px4Controller->deleteLater();
        px4Controller = nullptr;
    }
}

/**
 * @brief 初始化UI界面布局
 * @details 通过纯代码方式创建所有控件和布局，包含以下区域：
 *          1. 顶部标题栏（标题和编号）
 *          2. 第二行标题区
 *          3. 第一行控制按钮区（电源、起飞、指挥中心、降落、能源更换）
 *          4. 第二行（指南针、目的地输入、时间显示）
 *          5. 第三行（飞行高度、飞行速度、灯光控制）
 *          6. 第四行（经纬度、电池电量、鸣笛控制）
 *          7. 第五行（目的地距离、当前位置）
 */
void Widget::setupUI()  // setupUI()方法定义：初始化所有界面控件和布局
{
    // ==================== 创建主布局（垂直布局） ====================
    QVBoxLayout *mainLayout = new QVBoxLayout(this);           // 创建垂直布局对象，设置为当前窗口的主布局
    mainLayout->setContentsMargins(8, 6, 8, 6);               // 设置主布局的外边距：左右8、上下6像素
    mainLayout->setSpacing(4);                               // 设置布局中控件之间的间距为4像素

    // ==================== 顶部标题栏 ====================
    QFrame *topFrame = new QFrame;                            // 创建顶部标题栏框架对象，作为标题和编号的容器
    topFrame->setObjectName("topFrame");                      // 设置对象名为"topFrame"，用于QSS样式匹配
    QHBoxLayout *topLayout = new QHBoxLayout(topFrame);       // 创建水平布局对象，设置为topFrame的布局管理器
    topLayout->setContentsMargins(0, 0, 0, 0);               // 设置顶部布局的外边距为0，紧贴框架边缘
    topLayout->setSpacing(0);                                 // 设置顶部布局中控件间距为0
    
    // 创建主标题标签
    QLabel *titleLabel = new QLabel("飞行数据");               // 创建主标题标签，显示"飞行数据"文字
    titleLabel->setObjectName("titleLabel");                  // 设置对象名为"titleLabel"，用于QSS样式匹配
    
    // 创建编号标签
    QLabel *serialLabel = new QLabel("编号: TYFP0001CD00000001");  // 创建编号标签，显示设备唯一编号
    serialLabel->setObjectName("serialLabel");               // 设置对象名为"serialLabel"，用于QSS样式匹配
    
    // 将标题和编号添加到顶部布局（标题居中，编号往右移）
    topLayout->addStretch(3);                                // 添加左侧弹性空间（权重3），使标题偏右
    topLayout->addWidget(titleLabel);                         // 将标题标签添加到顶部布局
    topLayout->addStretch(1);                                // 标题右侧弹性空间（权重1）
    topLayout->addWidget(serialLabel);                        // 将编号标签添加到顶部布局
    topLayout->addStretch(1);                                // 编号右侧弹性空间（权重2），让编号往右移
    mainLayout->addWidget(topFrame);                          // 将顶部标题栏框架添加到主布局

    

    // ==================== 第一行控制按钮区（电源、起飞、悬停、降落、指挥中心、能源更换、更换航线） ====================
    QFrame *firstRowFrame = new QFrame;                       // 创建第一行按钮区框架对象，作为所有控制按钮的容器
    firstRowFrame->setObjectName("firstRowFrame");            // 设置对象名为"firstRowFrame"，用于QSS样式匹配
    QHBoxLayout *firstRowLayout = new QHBoxLayout(firstRowFrame);  // 创建水平布局对象，管理按钮的水平排列
    firstRowLayout->setContentsMargins(42, 6, 2, 6);        // 设置内边距：左42、上6、右2、下6像素
    firstRowLayout->setSpacing(12);                           // 设置控件间距为12像素

    // ==================== 创建圆形控制按钮 ====================
    // 电源按钮：控制设备电源开关
    btnPower = new QPushButton("电源");                        // 创建电源按钮，显示"电源"文字
    btnPower->setObjectName("roundButton");                   // 设置对象名为"roundButton"，应用圆形按钮样式
    btnPower->setFixedSize(55, 55);                           // 设置固定尺寸55x55像素，确保圆形显示
    connect(btnPower, &QPushButton::clicked, this, &Widget::onPowerClicked);  // 连接点击信号到电源处理槽函数
    
    // 起飞按钮：控制飞机起飞
    btnTakeoff = new QPushButton("起飞");                      // 创建起飞按钮，显示"起飞"文字
    btnTakeoff->setObjectName("roundButton");                 // 设置对象名为"roundButton"，应用圆形按钮样式
    btnTakeoff->setFixedSize(55, 55);                         // 设置固定尺寸55x55像素，确保圆形显示
    connect(btnTakeoff, &QPushButton::clicked, this, &Widget::onTakeoffClicked);  // 连接点击信号到起飞处理槽函数
    
    // 悬停按钮：控制飞机进入悬停状态
    btnHover = new QPushButton("悬停");                        // 创建悬停按钮，显示"悬停"文字
    btnHover->setObjectName("roundButton");                   // 设置对象名为"roundButton"，应用圆形按钮样式
    btnHover->setFixedSize(55, 55);                           // 设置固定尺寸55x55像素，确保圆形显示
    connect(btnHover, &QPushButton::clicked, this, &Widget::onHoverClicked);  // 连接点击信号到悬停处理槽函数
    
    // 降落按钮：控制飞机降落
    btnLand = new QPushButton("降落");                         // 创建降落按钮，显示"降落"文字
    btnLand->setObjectName("roundButton");                    // 设置对象名为"roundButton"，应用圆形按钮样式
    btnLand->setFixedSize(55, 55);                            // 设置固定尺寸55x55像素，确保圆形显示
    connect(btnLand, &QPushButton::clicked, this, &Widget::onLandClicked);  // 连接点击信号到降落处理槽函数
    
    // 指挥中心按钮：连接到指挥中心
    btnCommandCenter = new QPushButton("指挥\n中心");          // 创建指挥中心按钮，显示两行文字"指挥中心"
    btnCommandCenter->setObjectName("roundButton");           // 设置对象名为"roundButton"，应用圆形按钮样式
    btnCommandCenter->setFixedSize(55, 55);                   // 设置固定尺寸55x55像素，确保圆形显示
    connect(btnCommandCenter, &QPushButton::clicked, this, &Widget::onCommandCenterClicked);  // 连接点击信号到指挥中心处理槽函数
    
    // 结束按钮：结束飞行
    btnReturn = new QPushButton("结束");                       // 创建结束按钮，显示"结束"文字
    btnReturn->setObjectName("roundButton");                  // 设置对象名为"roundButton"，应用圆形按钮样式
    btnReturn->setFixedSize(55, 55);                          // 设置固定尺寸55x55像素，确保圆形显示
    connect(btnReturn, &QPushButton::clicked, this, &Widget::onReturnClicked);  // 连接点击信号到结束处理槽函数
    
    // ==================== 创建矩形能源更换按钮 ====================
    btnEnergyReplace = new QPushButton("能源更换");             // 创建能源更换按钮，显示"能源更换"文字
    btnEnergyReplace->setObjectName("rectButton");            // 设置对象名为"rectButton"，应用矩形按钮样式
    connect(btnEnergyReplace, &QPushButton::clicked, this, &Widget::onEnergyReplaceClicked);  // 连接点击信号到能源更换处理槽函数
    
    // ==================== 创建椭圆更换航线按钮 ====================
    btnRouteReplace = new QPushButton("更换航线");              // 创建更换航线按钮，显示"更换航线"文字
    btnRouteReplace->setObjectName("ellipseButton");          // 设置对象名为"ellipseButton"，应用椭圆按钮样式
    btnRouteReplace->setFixedSize(80, 40);                    // 设置固定尺寸80x40像素，确保椭圆显示
    btnRouteReplace->setStyleSheet("width: 80px; height: 40px; border-radius: 20px; "
                                   "border: 1.5px solid rgba(0, 212, 255, 0.7); border-top: 1.5px solid rgba(140, 215, 245, 0.9); border-left: 1.5px solid rgba(110, 200, 235, 0.85); border-right: 1.5px solid rgba(0, 80, 110, 0.9); border-bottom: 1.5px solid rgba(0, 55, 80, 0.95);"
                                   "background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 rgba(44, 58, 74, 0.9), stop:1 rgba(18, 24, 32, 0.95)); "
                                   "color: #00d4ff; font-size: 13px; font-weight: bold; text-align: center; "
                                   "box-shadow: inset 0 1px 2px rgba(0, 212, 255, 0.22), inset 0 -2px 3px rgba(0,0,0,0.45), 0 0 8px rgba(0, 212, 255, 0.12), 0 3px 8px rgba(0,0,0,0.6);");  // 立即应用样式显示椭圆形
    connect(btnRouteReplace, &QPushButton::clicked, this, &Widget::onRouteReplaceClicked);  // 连接点击信号到更换航线处理槽函数

    // ==================== 创建时间显示标签（支持多行） ====================
    lblTime = new QLabel("2026.07.28 下午: 18:18PM\n丙午年 77周年 黄帝纪年4724年");  // 创建时间标签，显示日期和农历信息
    lblTime->setObjectName("timeLabel");                      // 设置对象名为"timeLabel"，用于QSS样式匹配
    lblTime->setAlignment(Qt::AlignCenter);                   // 设置文字居中对齐
    lblTime->setWordWrap(true);                               // 启用自动换行，支持多行显示

    // ==================== 将按钮添加到布局（居中排列） ====================
    firstRowLayout->addStretch();                             // 添加左侧弹性空间，使按钮整体居中
    firstRowLayout->addWidget(btnPower);                      // 添加电源按钮到布局
    firstRowLayout->addWidget(btnTakeoff);                    // 添加起飞按钮到布局
    firstRowLayout->addWidget(btnHover);                      // 添加悬停按钮到布局
    firstRowLayout->addWidget(btnLand);                       // 添加降落按钮到布局
    firstRowLayout->addWidget(btnCommandCenter);              // 添加指挥中心按钮到布局
    firstRowLayout->addWidget(btnReturn);                    // 添加归还按钮到布局
    firstRowLayout->addWidget(btnEnergyReplace);              // 添加能源更换按钮到布局
    firstRowLayout->addSpacing(20);                           // 时间标签前增加10px固定间距，使其单独右移10px
    firstRowLayout->addWidget(lblTime);                       // 添加时间显示标签到布局（原更换航线位置）
    firstRowLayout->addStretch();                             // 添加右侧弹性空间，使按钮整体居中
    mainLayout->addWidget(firstRowFrame);                     // 将第一行按钮区框架添加到主布局

    // ==================== 第二行（指南针、目的地输入、更换航线、鸣笛、灯光） ====================
    QFrame *secondRowFrame = new QFrame;                      // 创建第二行框架对象，作为指南针、输入框、时间、鸣笛、灯光的容器
    secondRowFrame->setObjectName("secondRowFrame");          // 设置对象名为"secondRowFrame"，用于QSS样式匹配
    QHBoxLayout *secondRowLayout = new QHBoxLayout(secondRowFrame);  // 创建水平布局对象，管理控件的水平排列
    secondRowLayout->setContentsMargins(12, 6, 12, 6);       // 设置内边距：左12、上6、右12、下6像素
    secondRowLayout->setSpacing(12);                          // 设置控件间距为12像素

    // ==================== 创建指南针控件（直接显示指南针样式，不可点击） ====================
    compassWidget = new CompassWidget;                         // 创建指南针控件
    compassWidget->setFixedSize(95, 95);                      // 设置固定尺寸95x95像素
    
    // ==================== 创建目的地输入框 ====================
    leDestination = new QLineEdit();                          // 创建目的地输入框（初始为空）
    leDestination->setPlaceholderText("输入目的地");          // 设置占位提示文字：未输入时灰色显示，点击输入时自动消失，无需手动删除
    leDestination->setObjectName("destinationInput");         // 设置对象名为"destinationInput"，用于QSS样式匹配
    leDestination->setFixedSize(200, 40);                     // 设置固定尺寸200x40像素
    connect(leDestination, &QLineEdit::textChanged, this, &Widget::onDestinationTextChanged);  // 连接文本变化信号到处理槽函数
    leDestination->installEventFilter(this);  // 安装事件过滤器，用于捕获焦点事件
    
    // ==================== 创建鸣笛控制按钮（复合按钮） ====================
    btnHorn = new QPushButton("鸣笛");                         // 创建鸣笛控制按钮，显示"鸣笛"文字
    btnHorn->setObjectName("roundButton");                    // 设置对象名为"roundButton"，应用圆形按钮样式
    btnHorn->setFixedSize(55, 55);                            // 设置固定尺寸55x55像素，确保圆形显示
    btnHorn->setAutoRepeat(false);                            // 禁用自动重复
    connect(btnHorn, &QPushButton::pressed, this, &Widget::onHornPressed);  // 连接按压信号
    connect(btnHorn, &QPushButton::released, this, &Widget::onHornReleased);  // 连接释放信号
    
    // ==================== 创建灯光控制按钮（椭圆形） ====================
    btnLights = new QPushButton("灯光");                       // 创建灯光控制按钮，显示"灯光"文字
    btnLights->setObjectName("ellipseButton");                   // 设置对象名为"ovalButton"，应用椭圆形按钮样式
    btnLights->setFixedSize(65, 45);                          // 设置固定尺寸65x45像素，确保椭圆形显示
    connect(btnLights, &QPushButton::clicked, this, &Widget::onLightsClicked);  // 连接点击信号到灯光处理槽函数

    // ==================== 创建近光和远光按钮（在灯光右侧，垂直排布） ====================
    QWidget *lightControlFrame = new QWidget;                   // 使用QWidget作为容器
    lightControlFrame->setStyleSheet("background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #1c2128, stop:0.5 #161a20, stop:1 #0f1217);");  // 与界面背景一致
    QVBoxLayout *lightControlLayout = new QVBoxLayout(lightControlFrame);  // 创建垂直布局
    lightControlLayout->setContentsMargins(0, 0, 0, 0);        // 无边距
    lightControlLayout->setSpacing(4);                         // 按钮间距4像素

    btnLowLight = new QPushButton("近光");                     // 创建近光按钮
    btnLowLight->setObjectName("smallEllipseButton");          // 设置对象名，应用小椭圆形样式
    btnLowLight->setFixedSize(50, 35);                         // 设置较小尺寸50x35像素
    btnLowLight->setEnabled(false);                            // 初始禁用，灯光开启后才可用
    btnLowLight->setStyleSheet(lowLightOffStyle);              // 立即应用关闭状态样式，显示椭圆形
    connect(btnLowLight, &QPushButton::clicked, this, &Widget::onLowLightClicked);

    btnHighLight = new QPushButton("远光");                    // 创建远光按钮
    btnHighLight->setObjectName("smallEllipseButton");         // 设置对象名，应用小椭圆形样式
    btnHighLight->setFixedSize(50, 35);                        // 设置较小尺寸50x35像素
    btnHighLight->setEnabled(false);                           // 初始禁用，灯光开启后才可用
    btnHighLight->setStyleSheet(highLightOffStyle);            // 立即应用关闭状态样式，显示椭圆形
    connect(btnHighLight, &QPushButton::clicked, this, &Widget::onHighLightClicked);

    lightControlLayout->addWidget(btnLowLight);                // 添加近光按钮到垂直布局
    lightControlLayout->addWidget(btnHighLight);               // 添加远光按钮到垂直布局

    // ==================== 将控件添加到第二行布局 ====================
    secondRowLayout->addStretch();                            // 添加左侧弹性空间，使控件整体居中
    secondRowLayout->addWidget(compassWidget);                // 添加指南针控件（已通过样式左移20px）
    secondRowLayout->addWidget(leDestination);               // 添加目的地输入框到布局
    secondRowLayout->addWidget(btnRouteReplace);              // 添加更换航线按钮到布局（原时间显示位置）
    secondRowLayout->addWidget(btnHorn);                      // 添加鸣笛控制按钮到布局
    secondRowLayout->addWidget(btnLights);                    // 添加灯光控制按钮到布局
    secondRowLayout->addWidget(lightControlFrame);            // 添加近光/远光控制框架到布局（移到灯光右边）
    secondRowLayout->addStretch();                            // 添加右侧弹性空间，使控件整体居中
    mainLayout->addWidget(secondRowFrame);                    // 将第二行框架添加到主布局

    // ==================== 第三行（飞行高度、飞行速度、飞行时长） ====================
    QFrame *thirdRowFrame = new QFrame;                       // 创建第三行框架对象，作为高度、速度、时长信息框的容器
    thirdRowFrame->setObjectName("thirdRowFrame");            // 设置对象名为"thirdRowFrame"，用于QSS样式匹配
    QHBoxLayout *thirdRowLayout = new QHBoxLayout(thirdRowFrame);  // 创建水平布局对象，管理三个信息框的水平排列
    thirdRowLayout->setContentsMargins(12, 6, 12, 6);        // 设置内边距：左12、上6、右12、下6像素
    thirdRowLayout->setSpacing(12);                           // 设置控件间距为12像素

    // ==================== 创建飞行高度信息框 ====================
    QFrame *heightFrame = new QFrame;                         // 创建飞行高度信息框框架
    heightFrame->setObjectName("infoFrame");                  // 设置对象名为"infoFrame"，应用信息框样式
    QVBoxLayout *heightLayout = new QVBoxLayout(heightFrame); // 创建垂直布局对象，管理高度标签和按钮
    heightLayout->setContentsMargins(10, 6, 10, 6);          // 设置内边距：左10、上6、右10、下6像素
    heightLayout->setSpacing(5);                              // 设置控件间距为5像素
    
    // 添加科技感图标
    QHBoxLayout *heightTopLayout = new QHBoxLayout;           // 创建水平布局用于图标和标题
    QLabel *iconHeight = new QLabel;                          // 创建图标标签
    iconHeight->setPixmap(createTechIcon(0));                // 设置高度图标
    iconHeight->setFixedSize(24, 24);                         // 设置图标固定大小
    
    lblFlightHeight = new QLabel("飞行高度：800mm");           // 创建飞行高度显示标签，初始值为800mm
    lblFlightHeight->setObjectName("dataLabel");              // 设置对象名为"dataLabel"，应用数据标签样式
    
    heightTopLayout->addWidget(iconHeight);                   // 添加图标到布局
    heightTopLayout->addWidget(lblFlightHeight);              // 添加高度标签到布局
    heightTopLayout->addSpacing(10);                           // 添加间距
    // 创建0.6米和1.0米高度快捷按钮（放在高度标签右边）
    btn0m6 = new QPushButton("0.60m");                         // 创建0.6米高度快捷按钮
    btn0m6->setObjectName("smallBtn");                        // 设置对象名为"smallBtn"，应用小型按钮样式
    btn0m6->setEnabled(false);                                // 初始不可点击
    // 0.60m 高度快捷按钮：设置目标高度为 0.6m
    // PX4 飞行中点击时，通过 Offboard 模式调整无人机实际飞行高度
    connect(btn0m6, &QPushButton::clicked, [this]() {
        targetFlightHeight = 0.6;                                          // 更新目标高度变量
        flightHeight = 0.6;                                                // 立即更新当前高度（界面反馈）
        lblFlightHeight->setText("高度: "+QString::number(flightHeight, 'f', 2) + "m");
        // PX4 仿真模式且无人机正在飞行中：发送高度调整指令
        if (px4Connected && px4Controller && isFlying) {
            px4Controller->setAltitude(0.6);                               // 通过 Offboard 调整高度到 0.6m
        }
        setShortcutButtonBeige(btn0m6, heightBtnGroup);  // 设置当前按钮为米色高亮，同组其他按钮恢复默认
    });
    btn1m0 = new QPushButton("1.00m");                         // 创建1.0米高度快捷按钮
    btn1m0->setObjectName("smallBtn");                        // 设置对象名为"smallBtn"，应用小型按钮样式
    btn1m0->setEnabled(false);                                // 初始不可点击
    // 1.00m 高度快捷按钮：设置目标高度为 1.0m
    // PX4 飞行中点击时，通过 Offboard 模式调整无人机实际飞行高度
    connect(btn1m0, &QPushButton::clicked, [this]() {
        targetFlightHeight = 1.0;                                          // 更新目标高度变量
        flightHeight = 1.0;                                                // 立即更新当前高度（界面反馈）
        lblFlightHeight->setText("高度: "+QString::number(flightHeight, 'f', 2) + "m");
        // PX4 仿真模式且无人机正在飞行中：发送高度调整指令
        if (px4Connected && px4Controller && isFlying) {
            px4Controller->setAltitude(1.0);                               // 通过 Offboard 调整高度到 1.0m
        }
        setShortcutButtonBeige(btn1m0, heightBtnGroup);  // 设置当前按钮为米色高亮，同组其他按钮恢复默认
    });
    heightTopLayout->addWidget(btn0m6);                       // 将0.6米按钮添加到高度标签右边
    heightTopLayout->addWidget(btn1m0);                       // 将1.0米按钮添加到高度标签右边
    heightTopLayout->addStretch();                            // 添加弹性空间

    // 创建高度快捷设置按钮布局
    QHBoxLayout *heightBtnLayout = new QHBoxLayout;           // 创建水平布局用于放置高度快捷按钮
    heightBtnLayout->setSpacing(8);                          // 设置按钮间距为8像素
    btn1m5 = new QPushButton("1.50m");                         // 创建1.5米高度快捷按钮
    btn1m5->setObjectName("smallBtn");                        // 设置对象名为"smallBtn"，应用小型按钮样式
    btn1m5->setEnabled(false);                                // 初始不可点击
    // 1.50m 高度快捷按钮：设置目标高度为 1.5m
    // PX4 飞行中点击时，通过 Offboard 模式调整无人机实际飞行高度
    connect(btn1m5, &QPushButton::clicked, [this]() {
        targetFlightHeight = 1.5;                                          // 更新目标高度变量
        flightHeight = 1.5;                                                // 立即更新当前高度（界面反馈）
        lblFlightHeight->setText("高度: "+QString::number(flightHeight, 'f', 2) + "m");
        // PX4 仿真模式且无人机正在飞行中：发送高度调整指令
        if (px4Connected && px4Controller && isFlying) {
            px4Controller->setAltitude(1.5);                               // 通过 Offboard 调整高度到 1.5m
        }
        setShortcutButtonBeige(btn1m5, heightBtnGroup);  // 设置当前按钮为米色高亮，同组其他按钮恢复默认
    });
    btn2m = new QPushButton("2.00m");                          // 创建2.0米高度快捷按钮
    btn2m->setObjectName("smallBtn");                         // 设置对象名为"smallBtn"，应用小型按钮样式
    btn2m->setEnabled(false);                                 // 初始不可点击
    // 2.00m 高度快捷按钮：设置目标高度为 2.0m
    // PX4 飞行中点击时，通过 Offboard 模式调整无人机实际飞行高度
    connect(btn2m, &QPushButton::clicked, [this]() {
        targetFlightHeight = 2.0;                                          // 更新目标高度变量
        flightHeight = 2.0;                                                // 立即更新当前高度（界面反馈）
        lblFlightHeight->setText("高度: "+QString::number(flightHeight, 'f', 2) + "m");
        // PX4 仿真模式且无人机正在飞行中：发送高度调整指令
        if (px4Connected && px4Controller && isFlying) {
            px4Controller->setAltitude(2.0);                               // 通过 Offboard 调整高度到 2.0m
        }
        setShortcutButtonBeige(btn2m, heightBtnGroup);  // 设置当前按钮为米色高亮，同组其他按钮恢复默认
    });
    btn3m = new QPushButton("3.00m");                            // 创建3米高度快捷按钮
    btn3m->setObjectName("smallBtn");                         // 设置对象名为"smallBtn"，应用小型按钮样式
    btn3m->setEnabled(false);                                 // 初始不可点击
    // 3.00m 高度快捷按钮：设置目标高度为 3.0m
    // PX4 飞行中点击时，通过 Offboard 模式调整无人机实际飞行高度
    connect(btn3m, &QPushButton::clicked, [this]() {
        targetFlightHeight = 3.0;                                          // 更新目标高度变量
        flightHeight = 3.0;                                                // 立即更新当前高度（界面反馈）
        lblFlightHeight->setText("高度: "+QString::number(flightHeight, 'f', 2) + "m");
        // PX4 仿真模式且无人机正在飞行中：发送高度调整指令
        if (px4Connected && px4Controller && isFlying) {
            px4Controller->setAltitude(3.0);                               // 通过 Offboard 调整高度到 3.0m
        }
        setShortcutButtonBeige(btn3m, heightBtnGroup);  // 设置当前按钮为米色高亮，同组其他按钮恢复默认
    });
    heightBtnLayout->addWidget(btn1m5);                       // 将1.5米按钮添加到高度按钮布局
    heightBtnLayout->addWidget(btn2m);                        // 将2.0米按钮添加到高度按钮布局
    heightBtnLayout->addWidget(btn3m);                        // 将3米按钮添加到高度按钮布局

    // ==================== 设置高度快捷按钮为可选中并加入互斥分组 ====================
    btn0m6->setCheckable(true);                               // 设置0.6米按钮为可选中
    btn1m0->setCheckable(true);                               // 设置1.0米按钮为可选中
    btn1m5->setCheckable(true);                               // 设置1.5米按钮为可选中
    btn2m->setCheckable(true);                                // 设置2.0米按钮为可选中
    btn3m->setCheckable(true);                                // 设置3.0米按钮为可选中
    heightBtnGroup = new QButtonGroup(this);                  // 创建高度按钮互斥分组
    heightBtnGroup->setExclusive(true);                       // 设置为互斥模式，同一时间只能选中一个按钮
    heightBtnGroup->addButton(btn0m6);                        // 将0.6米按钮加入互斥分组
    heightBtnGroup->addButton(btn1m0);                        // 将1.0米按钮加入互斥分组
    heightBtnGroup->addButton(btn1m5);                        // 将1.5米按钮加入互斥分组
    heightBtnGroup->addButton(btn2m);                         // 将2.0米按钮加入互斥分组
    heightBtnGroup->addButton(btn3m);                         // 将3.0米按钮加入互斥分组
    
    heightLayout->addLayout(heightTopLayout);                  // 将图标和高度标签布局添加到高度信息框布局
    heightLayout->addLayout(heightBtnLayout);                 // 将高度按钮布局添加到高度信息框布局

    // ==================== 创建飞行速度信息框 ====================
    QFrame *speedFrame = new QFrame;                          // 创建飞行速度信息框框架
    speedFrame->setObjectName("infoFrame");                   // 设置对象名为"infoFrame"，应用信息框样式
    QVBoxLayout *speedLayout = new QVBoxLayout(speedFrame);   // 创建垂直布局对象，管理速度标签和按钮
    speedLayout->setContentsMargins(10, 6, 10, 6);           // 设置内边距：左10、上6、右10、下6像素
    speedLayout->setSpacing(5);                               // 设置控件间距为5像素

    // 添加科技感图标
    QHBoxLayout *speedTopLayout = new QHBoxLayout;            // 创建水平布局用于图标和标题
    QLabel *iconSpeed = new QLabel;                           // 创建图标标签
    iconSpeed->setPixmap(createTechIcon(1));                 // 设置速度图标
    iconSpeed->setFixedSize(24, 24);                          // 设置图标固定大小

    lblFlightSpeed = new QLabel("飞行速度:0.00km/h");          // 创建飞行速度显示标签，初始值为0.00km/h
    lblFlightSpeed->setObjectName("dataLabel");               // 设置对象名为"dataLabel"，应用数据标签样式

    // ==================== 创建速度快捷按钮（放在速度标签右边） ====================
    btn3km = new QPushButton("3.00km");                       // 创建3 km/h速度快捷按钮
    btn3km->setObjectName("smallBtn");                        // 设置对象名为"smallBtn"，应用小型按钮样式
    btn3km->setEnabled(false);                                // 初始不可点击
    connect(btn3km, &QPushButton::clicked, [this]() {         // 连接点击信号到设置最大速度处理
        maxFlightSpeed = 3;                                    // 设置最大速度为3 km/h
        setShortcutButtonBeige(btn3km, speedBtnGroup);  // 设置当前按钮为米色高亮，同组其他按钮恢复默认
    });
    btn5kmLow = new QPushButton("5.00km");                    // 创建5 km/h速度快捷按钮
    btn5kmLow->setObjectName("smallBtn");                     // 设置对象名为"smallBtn"，应用小型按钮样式
    btn5kmLow->setEnabled(false);                             // 初始不可点击
    connect(btn5kmLow, &QPushButton::clicked, [this]() {      // 连接点击信号到设置最大速度处理
        maxFlightSpeed = 5;                                    // 设置最大速度为5 km/h
        setShortcutButtonBeige(btn5kmLow, speedBtnGroup);  // 设置当前按钮为米色高亮，同组其他按钮恢复默认
    });

    speedTopLayout->addWidget(iconSpeed);                     // 添加图标到布局
    speedTopLayout->addWidget(lblFlightSpeed);                // 添加速度标签到布局
    speedTopLayout->addStretch(1);                            // 添加弹性空间，将速度快捷按钮向右推
    speedTopLayout->addWidget(btn3km);                        // 添加3 km/h速度快捷按钮到布局（速度标签右边）
    speedTopLayout->addWidget(btn5kmLow);                     // 添加5 km/h速度快捷按钮到布局（速度标签右边）

    // 创建速度快捷设置按钮布局（调整最大速度）
    QHBoxLayout *speedBtnLayout = new QHBoxLayout;            // 创建水平布局用于放置速度快捷按钮
    speedBtnLayout->setSpacing(8);                           // 设置按钮间距为8像素
    btn10m = new QPushButton("10.00km");                      // 创建10 km/h最大速度快捷按钮（原5.00km）
    btn10m->setObjectName("smallBtn");                        // 设置对象名为"smallBtn"，应用小型按钮样式
    btn10m->setEnabled(false);                                // 初始不可点击
    connect(btn10m, &QPushButton::clicked, [this]() {         // 连接点击信号到设置最大速度处理
        maxFlightSpeed = 10;                                   // 设置最大速度为10 km/h
        setShortcutButtonBeige(btn10m, speedBtnGroup);  // 设置当前按钮为米色高亮，同组其他按钮恢复默认
    });
    btn25km = new QPushButton("15.00km");                     // 创建15 km/h最大速度快捷按钮（原10.00km）
    btn25km->setObjectName("smallBtn");                       // 设置对象名为"smallBtn"，应用小型按钮样式
    btn25km->setEnabled(false);                               // 初始不可点击
    connect(btn25km, &QPushButton::clicked, [this]() {        // 连接点击信号到设置最大速度处理
        maxFlightSpeed = 15;                                   // 设置最大速度为15 km/h
        setShortcutButtonBeige(btn25km, speedBtnGroup);  // 设置当前按钮为米色高亮，同组其他按钮恢复默认
    });
    btn1km = new QPushButton("25.00km");                      // 创建25 km/h最大速度快捷按钮
    btn1km->setObjectName("smallBtn");                        // 设置对象名为"smallBtn"，应用小型按钮样式
    btn1km->setEnabled(false);                                // 初始不可点击
    connect(btn1km, &QPushButton::clicked, [this]() {         // 连接点击信号到设置最大速度处理
        maxFlightSpeed = 25;                                   // 设置最大速度为25 km/h
        setShortcutButtonBeige(btn1km, speedBtnGroup);  // 设置当前按钮为米色高亮，同组其他按钮恢复默认
    });
    btn5km = new QPushButton("30.00km");                      // 创建30 km/h最大速度快捷按钮
    btn5km->setObjectName("smallBtn");                        // 设置对象名为"smallBtn"，应用小型按钮样式
    btn5km->setEnabled(false);                                // 初始不可点击
    connect(btn5km, &QPushButton::clicked, [this]() {         // 连接点击信号到设置最大速度处理
        maxFlightSpeed = 30;                                   // 设置最大速度为30 km/h
        setShortcutButtonBeige(btn5km, speedBtnGroup);  // 设置当前按钮为米色高亮，同组其他按钮恢复默认
    });
    speedBtnLayout->addWidget(btn10m);                        // 将10 km/h按钮添加到速度按钮布局
    speedBtnLayout->addWidget(btn25km);                       // 将15 km/h按钮添加到速度按钮布局
    speedBtnLayout->addWidget(btn1km);                        // 将25 km/h按钮添加到速度按钮布局
    speedBtnLayout->addWidget(btn5km);                        // 将30 km/h按钮添加到速度按钮布局

    // ==================== 设置速度快捷按钮为可选中并加入互斥分组 ====================
    btn3km->setCheckable(true);                               // 设置3 km/h按钮为可选中
    btn5kmLow->setCheckable(true);                            // 设置5 km/h按钮为可选中
    btn10m->setCheckable(true);                               // 设置10 km/h按钮为可选中
    btn25km->setCheckable(true);                              // 设置15 km/h按钮为可选中
    btn1km->setCheckable(true);                               // 设置25 km/h按钮为可选中
    btn5km->setCheckable(true);                               // 设置30 km/h按钮为可选中
    speedBtnGroup = new QButtonGroup(this);                   // 创建速度按钮互斥分组
    speedBtnGroup->setExclusive(true);                        // 设置为互斥模式，同一时间只能选中一个按钮
    speedBtnGroup->addButton(btn3km);                         // 将3 km/h按钮加入互斥分组
    speedBtnGroup->addButton(btn5kmLow);                      // 将5 km/h按钮加入互斥分组
    speedBtnGroup->addButton(btn10m);                         // 将10 km/h按钮加入互斥分组
    speedBtnGroup->addButton(btn25km);                        // 将15 km/h按钮加入互斥分组
    speedBtnGroup->addButton(btn1km);                         // 将25 km/h按钮加入互斥分组
    speedBtnGroup->addButton(btn5km);                         // 将30 km/h按钮加入互斥分组

    // ==================== 初始化：为所有快捷键应用默认内联样式 ====================
    // 关键：从按钮创建之初就使用内联样式，而非依赖QSS
    // 这样切换默认/米色状态时始终走同一条渲染路径，彻底避免尺寸不一致问题
    {
        QString defaultStyle = buildShortcutBtnDefaultStyle();  // 构建默认内联样式字符串
        btn0m6->setStyleSheet(defaultStyle);                    // 0.60m按钮
        btn1m0->setStyleSheet(defaultStyle);                    // 1.00m按钮
        btn1m5->setStyleSheet(defaultStyle);                    // 1.50m按钮
        btn2m->setStyleSheet(defaultStyle);                     // 2.00m按钮
        btn3m->setStyleSheet(defaultStyle);                     // 3.00m按钮
        btn3km->setStyleSheet(defaultStyle);                    // 3.00km按钮
        btn5kmLow->setStyleSheet(defaultStyle);                  // 5.00km按钮
        btn10m->setStyleSheet(defaultStyle);                    // 10.00km按钮
        btn25km->setStyleSheet(defaultStyle);                   // 15.00km按钮
        btn1km->setStyleSheet(defaultStyle);                    // 25.00km按钮
        btn5km->setStyleSheet(defaultStyle);                    // 30.00km按钮
    }

    speedLayout->addLayout(speedTopLayout);                    // 将图标和速度标签布局添加到速度信息框布局
    speedLayout->addLayout(speedBtnLayout);                   // 将速度按钮布局添加到速度信息框布局

    // ==================== 创建飞行时长信息框 ====================
    QFrame *durationFrame = new QFrame;                       // 创建飞行时长信息框框架
    durationFrame->setObjectName("infoFrame");                // 设置对象名为"infoFrame"，应用信息框样式
    QVBoxLayout *durationLayout = new QVBoxLayout(durationFrame); // 创建垂直布局对象，管理时长标签和数值
    durationLayout->setContentsMargins(10, 6, 10, 6);        // 设置内边距：左10、上6、右10、下6像素
    durationLayout->setSpacing(5);                            // 设置控件间距为5像素

    // 第一行：图标 + 总飞行时长标题 + 总飞行时长数值（同一水平行）
    QHBoxLayout *durationTopLayout = new QHBoxLayout;         // 创建水平布局用于图标、标题和数值
    QLabel *iconDuration = new QLabel;                        // 创建图标标签
    iconDuration->setPixmap(createTechIcon(2));              // 设置时长图标
    iconDuration->setFixedSize(24, 24);                       // 设置图标固定大小

    lblFlightDuration = new QLabel("总飞行时长:");             // 创建总飞行时长标签
    lblFlightDuration->setObjectName("dataLabel");            // 设置对象名为"dataLabel"，应用数据标签样式

    lblDurationValue = new QLabel("00:00:00");               // 创建总飞行时长数值标签，初始值为00:00:00
    lblDurationValue->setObjectName("batteryPercent");       // 设置对象名为"batteryPercent"，复用电池百分比样式

    durationTopLayout->addWidget(iconDuration);               // 添加图标到布局
    durationTopLayout->addWidget(lblFlightDuration);          // 添加总飞行时长标签到布局
    durationTopLayout->addWidget(lblDurationValue);           // 添加总飞行时长数值到布局（与文字同一行）
    durationTopLayout->addStretch();                          // 添加弹性空间

    // 第二行：当前飞行时长标题 + 当前飞行时长数值（同一水平行）
    QHBoxLayout *currentDurationLayout = new QHBoxLayout;    // 创建水平布局用于当前飞行时长
    QLabel *lblCurrentDuration = new QLabel("当前飞行时长:");  // 创建当前飞行时长标签
    lblCurrentDuration->setObjectName("dataLabel");          // 设置对象名为"dataLabel"，应用数据标签样式

    lblCurrentDurationValue = new QLabel("00:00:00");        // 创建当前飞行时长数值标签，初始值为00:00:00
    lblCurrentDurationValue->setObjectName("batteryPercent"); // 设置对象名为"batteryPercent"，复用电池百分比样式

    currentDurationLayout->addSpacing(24);                   // 留出与图标对齐的空间
    currentDurationLayout->addWidget(lblCurrentDuration);    // 添加当前飞行时长标签到布局
    currentDurationLayout->addWidget(lblCurrentDurationValue); // 添加当前飞行时长数值到布局（与文字同一行）
    currentDurationLayout->addStretch();                     // 添加弹性空间

    durationLayout->addLayout(durationTopLayout);             // 将总飞行时长行添加到时长信息框布局
    durationLayout->addLayout(currentDurationLayout);         // 将当前飞行时长行添加到时长信息框布局

    // ==================== 将三个信息框添加到第三行布局 ====================
    thirdRowLayout->addWidget(heightFrame);                   // 添加飞行高度信息框到第三行布局
    thirdRowLayout->addWidget(speedFrame);                    // 添加飞行速度信息框到第三行布局
    thirdRowLayout->addWidget(durationFrame);                 // 添加飞行时长信息框到第三行布局
    mainLayout->addWidget(thirdRowFrame);                     // 将第三行框架添加到主布局

    // ==================== 第四行（经纬度、电池电量、重量显示） ====================
    QFrame *fourthRowFrame = new QFrame;                      // 创建第四行框架对象，作为经纬度、电池、重量信息框的容器
    fourthRowFrame->setObjectName("fourthRowFrame");          // 设置对象名为"fourthRowFrame"，用于QSS样式匹配
    QHBoxLayout *fourthRowLayout = new QHBoxLayout(fourthRowFrame);  // 创建水平布局对象，管理三个信息框的水平排列
    fourthRowLayout->setContentsMargins(12, 6, 12, 6);       // 设置内边距：左12、上6、右12、下6像素
    fourthRowLayout->setSpacing(12);                          // 设置控件间距为12像素

    // ==================== 创建经纬度信息框 ====================
    QFrame *coordFrame = new QFrame;                          // 创建经纬度信息框框架
    coordFrame->setObjectName("infoFrame");                   // 设置对象名为"infoFrame"，应用信息框样式
    QVBoxLayout *coordLayout = new QVBoxLayout(coordFrame);   // 创建垂直布局对象，管理经纬度标签
    coordLayout->setContentsMargins(10, 6, 10, 6);           // 设置内边距：左10、上6、右10、下6像素
    
    // 添加科技感图标
    QHBoxLayout *coordTopLayout = new QHBoxLayout;            // 创建水平布局用于图标和标题
    QLabel *iconCoord = new QLabel;                           // 创建图标标签
    iconCoord->setPixmap(createTechIcon(3));                 // 设置坐标图标
    iconCoord->setFixedSize(24, 24);                          // 设置图标固定大小
    
    QLabel *coordTitle = new QLabel("经纬度");                 // 创建经纬度标题标签
    coordTitle->setObjectName("dataLabel");                   // 设置对象名为"dataLabel"，应用数据标签样式
    
    coordTopLayout->addWidget(iconCoord);                     // 添加图标到布局
    coordTopLayout->addWidget(coordTitle);                    // 添加标题到布局
    coordTopLayout->addStretch();                             // 添加弹性空间
    
    lblCoordinates = new QLabel("LAT 30°39'54.6\"\nNLON 104°03'883\"E");  // 创建经纬度标签，显示纬度和经度
    lblCoordinates->setObjectName("dataLabel");               // 设置对象名为"dataLabel"，应用数据标签样式
    lblCoordinates->setWordWrap(true);                        // 启用自动换行，支持多行显示
    
    coordLayout->addLayout(coordTopLayout);                   // 将图标和标题布局添加到经纬度信息框布局
    coordLayout->addWidget(lblCoordinates);                   // 将经纬度标签添加到经纬度信息框布局

    // ==================== 创建目的地距离信息框（左侧：航程信息） ====================
    QFrame *distanceFrame = new QFrame;                       // 创建目的地距离信息框框架
    distanceFrame->setObjectName("infoFrame");                // 设置对象名为"infoFrame"，应用信息框样式

    // ===== 左侧：航程信息区域 =====
    QVBoxLayout *distanceLeftLayout = new QVBoxLayout(distanceFrame);  // 创建左侧垂直布局（航程信息）
    distanceLeftLayout->setContentsMargins(10, 6, 10, 6);     // 设置内边距：左10、上6、右10、下6像素
    distanceLeftLayout->setSpacing(4);                         // 控件间距为4像素
    
    // 添加科技感图标和标题
    QHBoxLayout *distanceTopLayout = new QHBoxLayout;         // 创建水平布局用于图标和标题
    distanceTopLayout->setSpacing(4);                        // 设置图标和标题间距
    QLabel *iconDistance = new QLabel;                        // 创建图标标签
    iconDistance->setPixmap(createTechIcon(4));              // 设置距离图标
    iconDistance->setFixedSize(24, 24);                       // 设置图标固定大小
    
    QLabel *titleDistance = new QLabel("航程信息");            // 创建标题标签
    titleDistance->setObjectName("dataLabel");                // 设置对象名为"dataLabel"，应用数据标签样式
    
    distanceTopLayout->addWidget(iconDistance);               // 添加图标到布局
    distanceTopLayout->addWidget(titleDistance);              // 添加标题到布局
    distanceTopLayout->addStretch();                          // 添加弹性空间
    
    distanceLeftLayout->addLayout(distanceTopLayout);         // 将图标和标题布局添加到左侧布局

    // 三个距离标签放入子布局
    QVBoxLayout *distanceDataLayout = new QVBoxLayout;        // 创建距离数据子布局
    distanceDataLayout->setContentsMargins(8, 0, 0, 0);     // 左边距8像素，使数据右移
    distanceDataLayout->setSpacing(4);                        // 设置间距

    // 总距离显示
    lblTotalDistance = new QLabel("总行驶距离：0.00km");            // 创建总距离标签
    lblTotalDistance->setObjectName("dataLabel");             // 设置对象名为"dataLabel"，应用数据标签样式
    distanceDataLayout->addWidget(lblTotalDistance);

    // 已行驶距离显示
    lblTraveledDistance = new QLabel("已行驶距离：0.00km");        // 创建已行驶距离标签
    lblTraveledDistance->setObjectName("dataLabel");          // 设置对象名为"dataLabel"，应用数据标签样式
    distanceDataLayout->addWidget(lblTraveledDistance);

    // 目的地距离显示
    lblDistance = new QLabel("目的地距离：0.00km");            // 创建目的地距离标签
    lblDistance->setObjectName("dataLabel");                  // 设置对象名为"dataLabel"，应用数据标签样式
    distanceDataLayout->addWidget(lblDistance);

    distanceLeftLayout->addLayout(distanceDataLayout);        // 将距离数据子布局添加到左侧布局

    // ==================== 创建雷达信息框（右侧：雷达扫描图形） ====================
    QFrame *radarFrame = new QFrame;                         // 创建雷达信息框框架
    radarFrame->setObjectName("infoFrame");                   // 设置对象名为"infoFrame"，应用信息框样式
    QHBoxLayout *radarLayout = new QHBoxLayout(radarFrame);   // 创建雷达信息框布局
    radarLayout->setContentsMargins(10, 6, 10, 6);           // 设置内边距
    radarLayout->setSpacing(5);                               // 控件间距为5像素

    // ===== 左侧：卫星导航系统信息（竖列排序，名称+数量） =====
    QVBoxLayout *navLayout = new QVBoxLayout;                 // 创建导航系统垂直布局
    navLayout->setSpacing(3);                                 // 标签间距3像素

    // 北斗
    QLabel *lblBeidou = new QLabel("北斗：");                   // 创建北斗标签
    lblBeidou->setObjectName("dataLabel");                   // 应用数据标签样式
    lblBeidouCount = new QLabel("0颗", radarFrame);           // 创建北斗卫星数量标签，父对象为radarFrame可自由定位
    lblBeidouCount->setStyleSheet("color: #00ff66; font-size: 15px; font-weight: bold;");
    navLayout->addWidget(lblBeidou);                         // 添加北斗名称

    // GPS
    QLabel *lblGPS = new QLabel("GPS：");                       // 创建GPS标签
    lblGPS->setObjectName("dataLabel");                       // 应用数据标签样式
    lblGPSCount = new QLabel("0颗", radarFrame);               // 创建GPS卫星数量标签，父对象为radarFrame可自由定位
    lblGPSCount->setStyleSheet("color: #00ff66; font-size: 15px; font-weight: bold;");
    navLayout->addWidget(lblGPS);                             // 添加GPS名称

    // GLONASS
    QLabel *lblGLONASS = new QLabel("GLONASS：");               // 创建格洛纳斯标签
    lblGLONASS->setObjectName("dataLabel");                   // 应用数据标签样式
    lblGLONASSCount = new QLabel("0颗", radarFrame);           // 创建GLONASS卫星数量标签，父对象为radarFrame可自由定位
    lblGLONASSCount->setStyleSheet("color: #00ff66; font-size: 15px; font-weight: bold;");
    navLayout->addWidget(lblGLONASS);                         // 添加GLONASS名称

    // 伽利略
    QLabel *lblGalileo = new QLabel("伽利略：");                 // 创建伽利略标签
    lblGalileo->setObjectName("dataLabel");                   // 应用数据标签样式
    lblGalileoCount = new QLabel("0颗", radarFrame);           // 创建伽利略卫星数量标签，父对象为radarFrame可自由定位
    lblGalileoCount->setStyleSheet("color: #00ff66; font-size: 15px; font-weight: bold;");
    navLayout->addWidget(lblGalileo);                         // 添加伽利略名称

    navLayout->addStretch();                                  // 添加弹性空间，顶部对齐

    radarLayout->addLayout(navLayout);                        // 添加导航系统信息到左侧

    // 布局完成后通过定时器自由定位各卫星数量标签
    QTimer::singleShot(0, this, [this]() {
        // 在此处可自由调整各数量标签的位置，move(x, y)中x控制水平位置，y控制垂直位置
        if (lblBeidouCount)    lblBeidouCount->move(65, 3);
        if (lblGPSCount)       lblGPSCount->move(65, 28);
        if (lblGLONASSCount)   lblGLONASSCount->move(100, 56);
        if (lblGalileoCount)   lblGalileoCount->move(65, 83);
    });

    // ===== 右侧：雷达扫描图形（右移5像素） =====
    QHBoxLayout *radarShiftLayout = new QHBoxLayout;          // 创建雷达偏移布局
    radarShiftLayout->setContentsMargins(5, 0, 0, 0);         // 左边距5像素，使雷达右移
    radarShiftLayout->setSpacing(0);                          // 无间距

    radarWidget = new RadarWidget(radarFrame);                // 创建雷达扫描控件，父对象为radarFrame
    radarWidget->setFixedSize(110, 110);                        // 设置雷达控件固定尺寸为85x85像素
    radarWidget->setScanSpeed(120);                           // 设置扫描速度为120度/秒
    radarShiftLayout->addWidget(radarWidget, 0, Qt::AlignCenter);  // 添加雷达控件，居中对齐

    radarLayout->addLayout(radarShiftLayout);                 // 添加雷达偏移布局到主布局

    // ==================== 创建重量显示信息框 ====================
    QFrame *weightFrame = new QFrame;                         // 创建重量显示信息框框架
    weightFrame->setObjectName("infoFrame");                  // 设置对象名为"infoFrame"，应用信息框样式
    QVBoxLayout *weightLayout = new QVBoxLayout(weightFrame); // 创建垂直布局对象，管理重量标签和数值
    weightLayout->setContentsMargins(10, 6, 10, 6);          // 设置内边距：左10、上6、右10、下6像素
    weightLayout->setSpacing(5);                              // 设置控件间距为5像素

    // 添加科技感图标 + 限重（同一行）
    QHBoxLayout *weightTopLayout = new QHBoxLayout;           // 创建水平布局用于图标和限重
    weightTopLayout->setSpacing(4);                           // 图标和限重间距4像素
    QLabel *iconWeight = new QLabel;                          // 创建图标标签
    iconWeight->setPixmap(createTechIcon(5));                // 设置重量图标
    iconWeight->setFixedSize(24, 24);                         // 设置图标固定大小
    lblWeightValue = new QLabel("限重120.00kg");              // 创建限重标签，初始值为120.00kg
    lblWeightValue->setObjectName("batteryPercent");          // 设置对象名为"batteryPercent"，复用电池百分比样式
    weightTopLayout->addWidget(iconWeight);                   // 添加图标到布局
    weightTopLayout->addWidget(lblWeightValue);               // 添加限重标签到布局
    weightTopLayout->addStretch();                            // 添加弹性空间

    // 空重显示（固定值）
    QLabel *lblEmptyWeight = new QLabel("空重: 40.00kg");       // 创建空重标签，固定为40.00kg
    lblEmptyWeight->setObjectName("dataLabel");                // 设置对象名为"dataLabel"，应用数据标签样式

    // 有效载重显示（固定值）
    QLabel *lblPayloadWeight = new QLabel("有效载重: 80.00kg"); // 创建有效载重标签，固定为80.00kg
    lblPayloadWeight->setObjectName("dataLabel");              // 设置对象名为"dataLabel"，应用数据标签样式

    // 总载重显示（空重+当前实际重量）
    lblWeight = new QLabel("总载重: --");                      // 创建总载重标签
    lblWeight->setObjectName("dataLabel");                    // 设置对象名为"dataLabel"，应用数据标签样式

    weightLayout->addLayout(weightTopLayout);                 // 将图标+限重布局添加到重量信息框布局
    weightLayout->addWidget(lblEmptyWeight);                  // 将空重标签添加到布局（第一行数据）
    weightLayout->addWidget(lblPayloadWeight);                // 将有效载重标签添加到布局（第二行数据）
    weightLayout->addWidget(lblWeight);                       // 将总载重标签添加到布局（第三行数据）

    // ==================== 将四个信息框添加到第四行布局 ====================
    // 通过setFixedWidth设置具体宽度，可自由调整各框大小
    coordFrame->setFixedWidth(180);                            // 经纬度信息框固定宽度180像素
    distanceFrame->setFixedWidth(220);                        // 航程信息框固定宽度300像素
    radarFrame->setFixedWidth(250);                            // 雷达信息框固定宽度200像素
    // weightFrame使用弹性宽度，铺满剩余空间
    fourthRowLayout->addWidget(coordFrame);                    // 添加经纬度信息框
    fourthRowLayout->addWidget(distanceFrame);                 // 添加航程信息框
    fourthRowLayout->addWidget(radarFrame);                    // 添加雷达信息框
    fourthRowLayout->addWidget(weightFrame, 1);               // 添加重量信息框（弹性铺满剩余）
    mainLayout->addWidget(fourthRowFrame);                    // 将第四行框架添加到主布局

    // ==================== 第五行（电池电量、当前位置） ====================
    QFrame *fifthRowFrame = new QFrame;                       // 创建第五行框架对象，作为电池电量和当前位置信息框的容器
    fifthRowFrame->setObjectName("fifthRowFrame");            // 设置对象名为"fifthRowFrame"，用于QSS样式匹配
    QHBoxLayout *fifthRowLayout = new QHBoxLayout(fifthRowFrame);  // 创建水平布局对象，管理两个信息框的水平排列
    fifthRowLayout->setContentsMargins(12, 6, 12, 6);        // 设置内边距：左12、上6、右12、下6像素
    fifthRowLayout->setSpacing(12);                           // 设置控件间距为12像素

    // ==================== 创建电池电量信息框（缩小） ====================
    QFrame *batteryFrame = new QFrame;                        // 创建电池电量信息框框架
    batteryFrame->setObjectName("infoFrame");                  // 设置对象名为"infoFrame"，应用信息框样式
    QVBoxLayout *batteryLayout = new QVBoxLayout(batteryFrame);  // 创建垂直布局对象，三层结构：电量、进度条、色块
    batteryLayout->setContentsMargins(10, 6, 10, 6);            // 设置内边距：左10、上6、右10、下6像素
    batteryLayout->setSpacing(8);                              // 设置控件间距为8像素
    
    // ==================== 第一层：图标、电量标签和百分比（横向显示） ====================
    QHBoxLayout *batteryTopLayout = new QHBoxLayout;          // 创建水平布局用于图标、电量标签和百分比
    batteryTopLayout->setSpacing(6);                           // 设置间距为6像素
    
    // 添加科技感图标
    QLabel *iconBattery = new QLabel;                         // 创建图标标签
    iconBattery->setPixmap(createTechIcon(6));                // 设置电池图标
    iconBattery->setFixedSize(24, 24);                        // 设置图标固定大小
    
    lblBatteryLevel = new QLabel("电量");                      // 创建电池电量标签
    lblBatteryLevel->setObjectName("dataLabel");              // 设置对象名为"dataLabel"，应用数据标签样式
    
    lblBatteryPercent = new QLabel("0%");                     // 创建电量百分比标签，初始值为0%
    lblBatteryPercent->setObjectName("batteryPercent");       // 设置对象名为"batteryPercent"，用于QSS样式匹配

    lblFlyableDistance = new QLabel;                             // 创建可飞行距离标签，与电量同一水平显示
    lblFlyableDistance->setObjectName("flyableDistance");     // 设置对象名为"flyableDistance"，用于QSS样式匹配
    lblFlyableDistance->setTextFormat(Qt::RichText);          // 启用富文本支持，实现多颜色显示
    lblFlyableDistance->setText("<span>可飞行距离：</span><span style='color:#00e676;'>0.00km</span>");

    batteryTopLayout->addWidget(iconBattery);                 // 添加图标到第一层布局
    batteryTopLayout->addWidget(lblBatteryLevel);             // 将电量标签添加到第一层布局
    batteryTopLayout->addWidget(lblBatteryPercent);           // 将电量百分比标签添加到第一层布局
    batteryTopLayout->addSpacing(5);                           // 向右偏移5像素
    batteryTopLayout->addWidget(lblFlyableDistance);          // 将可飞行距离标签添加到第一层布局（电量右边，同一水平）
    batteryTopLayout->addStretch();                            // 添加弹性空间，使内容左对齐
    
    // ==================== 第二层：电池能量条（总电量） ====================
    batteryBar = new QProgressBar;                            // 创建电量进度条对象
    batteryBar->setRange(0, 100);                             // 设置进度条范围：0~100
    batteryBar->setValue(100);                               // 设置进度条初始值为100%
    batteryBar->setTextVisible(false);                        // 隐藏进度条默认文字

    // ==================== 第三层：6个独立电池（从左到右依次消耗） ====================
    QHBoxLayout *batteryBarLayout = new QHBoxLayout;          // 创建水平布局
    batteryBarLayout->setSpacing(3);                           // 电池之间的间距

    // 创建6个独立电池条
    for (int i = 0; i < 6; i++) {
        batteryBars[i] = new QFrame;
        batteryBars[i]->setObjectName(QString("batteryBar%1").arg(i + 1));
        batteryBars[i]->setFixedHeight(14);
        batteryBars[i]->setFixedWidth(30);
        batteryBars[i]->setStyleSheet(
            "background: #00e676; border-radius: 4px; border: 1px solid rgba(0, 230, 118, 0.7);");
        batteryBarLayout->addWidget(batteryBars[i]);
    }

    // 当前电池编号标签
    lblBatteryIndex = new QLabel("电池 1/6", batteryFrame);
    lblBatteryIndex->setStyleSheet(
        "color: #00d4ff; font-size: 11px; padding: 2px 6px; "
        "border: 1px solid rgba(0, 212, 255, 0.5); border-radius: 3px; "
        "background: rgba(0, 212, 255, 0.08);");
    batteryBarLayout->addSpacing(6);
    batteryBarLayout->addWidget(lblBatteryIndex);
    batteryBarLayout->addStretch();
    
    // 将三层布局添加到电池信息框
    batteryLayout->addLayout(batteryTopLayout);               // 添加第一层：电量标签和百分比
    batteryLayout->addWidget(batteryBar);                     // 添加第二层：电池能量条
    batteryLayout->addLayout(batteryBarLayout);               // 添加第三层：五个电池色块

    // ==================== 创建地图信息框（第一部分：地图按钮和地图显示） ====================
    QFrame *mapFrame = new QFrame;                            // 创建地图信息框框架
    mapFrame->setObjectName("infoFrame");                      // 设置对象名为"infoFrame"，应用信息框样式
    positionFramePtr = mapFrame;                                // 保存到成员变量，便于几何位置计算
    QVBoxLayout *mapFrameLayout = new QVBoxLayout(mapFrame);  // 创建垂直布局对象，管理地图内容
    mapFrameLayout->setContentsMargins(8, 4, 8, 4);          // 设置内边距：左8、上4、右8、下4像素
    mapFrameLayout->setSpacing(4);                            // 设置控件间距为4像素

    // 添加可自由定位的科技感图标和标题（作为 mapFrame 子控件，不参与布局管理）
    // 通过 mapIconLabel->move(x, y) 和 mapTitleLabel->move(x, y) 可在代码中自由调整位置
    mapIconLabel = new QLabel(mapFrame);                       // 创建图标标签，父控件为 mapFrame
    mapIconLabel->setPixmap(createTechIcon(7));               // 设置地图图标
    mapIconLabel->setFixedSize(24, 24);                        // 设置图标固定大小
    mapIconLabel->move(0, 6);                                  // 初始位置：左上角
    mapIconLabel->setStyleSheet("background: transparent;");

    mapTitleLabel = new QLabel("地图", mapFrame);              // 创建地图标题标签，父控件为 mapFrame
    mapTitleLabel->setObjectName("dataLabel");                // 设置对象名为"dataLabel"，应用数据标签样式
    mapTitleLabel->move(21, 3);                                // 初始位置：图标右侧
    mapTitleLabel->setStyleSheet("background: transparent;");
    
    // ==================== 创建地图显示区域（增大） ====================
    QHBoxLayout *mapContentLayout = new QHBoxLayout;          // 创建水平布局用于地图和按钮
    mapContentLayout->setSpacing(4);                          // 设置控件间距为4像素
    
    // ==================== 创建占位控件（占据地图显示区域大小） ====================
    // 关键：用一个空QWidget占住layout中的位置和尺寸（不做任何绘制），实际地图显示由顶层工具子窗口（positionWebView）负责。
    // 这样QWebEngineView走与mapDialog一致的顶层窗口渲染路径，彻底避免子控件嵌入时的合成空白问题。
    positionMapPlaceholder = new QWidget;                      // 创建占位控件
    positionMapPlaceholder->setMinimumSize(120, 60);           // 最小尺寸适配拆分后的地图框宽度
    positionMapPlaceholder->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);  // 弹性扩展，铺满空间
    // 占位控件本身只做几何参考，外观透明（让positionWebView的视觉边框覆盖它）
    positionMapPlaceholder->setStyleSheet("background: transparent;");

    // ==================== 创建动态地图显示区域（子窗口方案，嵌入地图框内） ====================
    // 核心思路：positionWebView 作为 mapFrame 的子窗口，自动受父窗口裁剪约束，
    //   确保地图始终显示在地图框内部，不会溢出到相邻的视频框。
    //   通过 syncPositionMapGeometry() 将其几何位置同步到占位控件。
    positionWebView = new QWebEngineView(mapFrame);   // 作为 mapFrame 子窗口，自动受父窗口裁剪
    positionWebView->setStyleSheet("background: #0a1628;");
    positionWebView->setMouseTracking(true);
    positionWebView->setAcceptDrops(false);
    // 初始隐藏，待电源开启且布局完成后显示
    positionWebView->hide();
    // 为占位控件安装事件过滤器，捕获其移动/尺寸变化，即时同步窗口几何位置
    positionMapPlaceholder->installEventFilter(this);

    // 使用 DebugWebPage（复用地图弹窗的日志转发页面），将 JS 控制台日志转发到 qDebug 便于调试
    positionWebPage = new DebugWebPage(positionWebView);
    positionWebView->setPage(positionWebPage);
    QWebEngineSettings *posSettings = positionWebPage->settings();
    posSettings->setAttribute(QWebEngineSettings::JavascriptEnabled, true);               // 启用 JavaScript
    posSettings->setAttribute(QWebEngineSettings::LocalContentCanAccessRemoteUrls, true); // 允许本地内容访问远程URL（加载高德API）
    posSettings->setAttribute(QWebEngineSettings::LocalContentCanAccessFileUrls, true);   // 允许本地内容访问文件URL
    posSettings->setAttribute(QWebEngineSettings::ScrollAnimatorEnabled, true);           // 启用滚动动画
    posSettings->setAttribute(QWebEngineSettings::LocalStorageEnabled, true);             // 启用本地存储
    posSettings->setAttribute(QWebEngineSettings::AutoLoadImages, true);                  // 自动加载图片

    // 连接页面加载完成信号到就绪检查
    connect(positionWebView, &QWebEngineView::loadFinished, this, [this](bool ok) {
        if (!ok) {
            qWarning() << "[位置地图] HTML 页面加载失败！";
            positionMapReady = true;  // 标记就绪避免死锁
            return;
        }
        qDebug() << "[位置地图] HTML 加载完成，启动就绪检查";
        positionRetryCount = 0;
        checkPositionMapReady();
    });

    // 注意：amap.html 页面的加载延迟到 showEvent 中执行，确保窗口已显示、
    // QWebEngineView 拥有真实尺寸后再初始化渲染表面，避免子控件渲染空白。

    // ==================== 创建地图按钮布局（垂直排列，缩小） ====================
    QVBoxLayout *mapBtnLayout = new QVBoxLayout;              // 创建垂直布局用于放置地图按钮
    mapBtnLayout->setSpacing(3);                              // 设置按钮间距为3像素
    QPushButton *btnMap = new QPushButton("地图");             // 创建地图按钮，显示"地图"文字
    btnMap->setFixedWidth(40);                                // 设置按钮固定宽度（缩小）
    btnMap->setStyleSheet("padding: 4px 8px; border-radius: 4px; border: 1px solid rgba(0, 212, 255, 0.55); border-top: 1px solid rgba(140, 215, 245, 0.85); border-left: 1px solid rgba(110, 200, 235, 0.8); border-right: 1px solid rgba(0, 80, 110, 0.85); border-bottom: 1px solid rgba(0, 55, 80, 0.9);"
                          "background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 rgba(38, 50, 64, 0.88), stop:1 rgba(18, 24, 32, 0.92)); "
                          "color: #00d4ff; font-size: 11px; font-weight: bold; "
                          "box-shadow: inset 0 1px 2px rgba(0, 212, 255, 0.2), 0 2px 4px rgba(0,0,0,0.5);");
    connect(btnMap, &QPushButton::clicked, this, &Widget::onMapClicked);  // 连接点击信号到地图处理槽函数
    mapBtnLayout->addWidget(btnMap);                          // 将地图按钮添加到地图按钮布局
    
    // 将地图按钮和占位控件添加到水平布局（按钮在左，占位控件权重1铺满）
    // positionWebView 是顶层工具子窗口，不加入layout，通过 syncPositionMapGeometry 同步位置到占位控件几何位置
    mapContentLayout->addLayout(mapBtnLayout);                 // 将地图按钮布局添加到布局（左侧）
    mapContentLayout->addWidget(positionMapPlaceholder, 1);  // 将占位控件添加到布局（权重1，铺满剩余空间）

    // ==================== 将控件添加到地图信息框布局 ====================
    mapFrameLayout->addLayout(mapContentLayout, 1);           // 添加地图和按钮布局（权重1，填充剩余空间）

    // ==================== 创建视频显示信息框（第二部分：视频显示窗口） ====================
    QFrame *videoFrame = new QFrame;                          // 创建视频显示信息框框架
    videoFrame->setObjectName("infoFrame");                    // 设置对象名为"infoFrame"，应用信息框样式
    QVBoxLayout *videoFrameLayout = new QVBoxLayout(videoFrame);  // 创建垂直布局对象，管理视频内容
    videoFrameLayout->setContentsMargins(8, 4, 8, 4);        // 设置内边距：左8、上4、右8、下4像素
    videoFrameLayout->setSpacing(4);                          // 设置控件间距为4像素

    // 创建视频显示窗口
    videoDisplayLabel = new QLabel;                            // 创建视频显示窗口标签
    videoDisplayLabel->setObjectName("videoDisplay");          // 设置对象名，便于样式应用
    videoDisplayLabel->setMinimumSize(200, 90);               // 设置最小尺寸，保证可见性
    videoDisplayLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);  // 弹性扩展，铺满空间
    videoDisplayLabel->setAlignment(Qt::AlignCenter);         // 内容居中对齐
    videoDisplayLabel->setStyleSheet(
        "QLabel#videoDisplay { "
        "background: #0a1628; "
        "border: 1px solid rgba(0,255,255,0.3); "
        "border-radius: 4px; "
        "color: #00d4ff; "
        "font-size: 12px; "
        "}"
    );
    videoDisplayLabel->setText("视频显示窗口");                // 初始占位文字
    videoDisplayLabel->setScaledContents(false);             // 关闭自动缩放（保持等比，由代码缩放后 setPixmap）

    // 将控件添加到视频信息框布局
    videoFrameLayout->addWidget(videoDisplayLabel, 1);        // 添加视频显示窗口（权重1，填充剩余空间）

    // ==================== 将三个信息框添加到第五行布局（电池缩小，地图和视频各占一部分） ====================
    fifthRowLayout->addWidget(batteryFrame, 1);               // 添加电池电量信息框到第五行布局（缩小，权重1）
    fifthRowLayout->addWidget(mapFrame, 2);                   // 添加地图信息框到第五行布局（权重2）
    fifthRowLayout->addWidget(videoFrame, 2);                 // 添加视频信息框到第五行布局（权重2）
    mainLayout->addWidget(fifthRowFrame);                     // 将第五行框架添加到主布局
    mainLayout->addStretch(1);                                // 添加弹性空间，确保所有内容完整显示
}

/**
 * @brief 应用界面样式
 * @details 设置渐变浅青光色科技风格的QSS样式表，包含以下样式定义：
 *          - 全局背景（渐变浅青色）
 *          - 顶部标题栏样式
 *          - 标签样式（标题、编号、数据等）
 *          - 各种按钮样式（圆形、大号圆形、矩形、小型按钮）
 *          - 输入框样式
 *          - 信息框样式
 *          - 电池电量条样式
 */
void Widget::applyStyles()  // applyStyles()方法定义
{
    // 高级飞控UI样式：深灰黑背景 + 霓虹青蓝边框 + 玻璃拟态面板 + 圆角卡片分区
    QString styleSheet =
        "Widget {"
        "    background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #141a20, stop:0.5 #0e1218, stop:1 #080c10);"
        "}"
        "QLabel {"
        "    background: transparent;"
        "}"
        // 顶部标题栏：玻璃拟态 + 霓虹青蓝描边 + 柔和高光
        "#topFrame {"
        "    background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 rgba(24, 32, 42, 0.92), stop:1 rgba(14, 20, 28, 0.88));"
        "    border: 1px solid rgba(0, 212, 255, 0.45);"
        "    border-top: 2px solid rgba(0, 212, 255, 0.85);"
        "    border-left: 1px solid rgba(0, 212, 255, 0.55);"
        "    border-right: 1px solid rgba(0, 212, 255, 0.35);"
        "    border-bottom: 1px solid rgba(0, 212, 255, 0.3);"
        "    border-radius: 8px;"
        "}"
        // 标题：加粗青蓝
        "#titleLabel {"
        "    color: #00d4ff;"
        "    font-size: 30px;"
        "    font-weight: bold;"
        "}"
        "#serialLabel {"
        "    color: #7fb3d5;"
        "    font-size: 15px;"
        "    font-weight: bold;"
        "    margin-top: 5px;"
        "    padding: 2px 8px;"
        "    border: 1px solid rgba(0, 212, 255, 0.45);"
        "    border-top: 1px solid rgba(0, 212, 255, 0.7);"
        "    border-left: 1px solid rgba(0, 212, 255, 0.55);"
        "    border-right: 1px solid rgba(0, 212, 255, 0.3);"
        "    border-bottom: 1px solid rgba(0, 212, 255, 0.25);"
        "    border-radius: 4px;"
        "    background: rgba(0, 212, 255, 0.1);"
        "}"
        // 数据标签：高对比浅白
        "#dataLabel {"
        "    color: #e8f4ff;"
        "    font-size: 15px;"
        "    font-weight: bold;"
        "}"
        // 电量百分比：绿色状态色
        "#batteryPercent {"
        "    color: #00e676;"
        "    font-size: 18px;"
        "    font-weight: bold;"
        "}"
        // 可飞行距离：白色字体，与电量同一水平，公里数据通过富文本显示绿色
        "#flyableDistance {"
        "    color: #ffffff;"
        "    font-size: 15px;"
        "    font-weight: bold;"
        "}"
        // 时间标签：霓虹青蓝胶囊 + 柔和高光
        "#timeLabel {"
        "    color: #b8d4e8;"
        "    font-size: 14px;"
        "    font-weight: bold;"
        "    padding: 2px 10px;"
        "    border: 1px solid rgba(0, 212, 255, 0.4);"
        "    border-top: 1px solid rgba(0, 212, 255, 0.65);"
        "    border-left: 1px solid rgba(0, 212, 255, 0.5);"
        "    border-right: 1px solid rgba(0, 212, 255, 0.28);"
        "    border-bottom: 1px solid rgba(0, 212, 255, 0.22);"
        "    border-radius: 6px;"
        "    background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 rgba(24, 34, 46, 0.85), stop:1 rgba(16, 22, 30, 0.75));"
        "}"
        // 目的地输入框：霓虹青蓝描边 + 柔和高光
        "#destinationInput {"
        "    background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 rgba(22, 30, 40, 0.92), stop:1 rgba(14, 20, 28, 0.88));"
        "    border: 1px solid rgba(0, 212, 255, 0.5);"
        "    border-top: 1px solid rgba(0, 212, 255, 0.75);"
        "    border-left: 1px solid rgba(0, 212, 255, 0.6);"
        "    border-right: 1px solid rgba(0, 212, 255, 0.35);"
        "    border-bottom: 1px solid rgba(0, 212, 255, 0.3);"
        "    border-radius: 6px;"
        "    color: #e8f4ff;"
        "    font-size: 15px;"
        "    font-weight: bold;"
        "    padding: 0 10px;"
        "    selection-background-color: rgba(0, 212, 255, 0.35);"
        "}"
        "#destinationInput:focus {"
        "    border: 1.5px solid #00d4ff;"
        "    border-top: 1.5px solid rgba(100, 230, 255, 1);"
        "    border-left: 1.5px solid rgba(60, 220, 255, 0.95);"
        "    border-right: 1.5px solid rgba(0, 170, 210, 0.9);"
        "    border-bottom: 1.5px solid rgba(0, 150, 190, 0.95);"
        "    background: rgba(20, 28, 38, 0.92);"
        "}"
        // 信息面板：霓虹青蓝描边 + 柔和高光 + 玻璃拟态圆角卡片
        "#infoFrame {"
        "    background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 rgba(32, 42, 54, 0.85), stop:0.5 rgba(22, 30, 40, 0.8), stop:1 rgba(14, 20, 28, 0.85));"
        "    border: 1px solid rgba(0, 212, 255, 0.42);"
        "    border-top: 1.5px solid rgba(0, 212, 255, 0.75);"
        "    border-left: 1px solid rgba(0, 212, 255, 0.55);"
        "    border-right: 1px solid rgba(0, 212, 255, 0.3);"
        "    border-bottom: 1px solid rgba(0, 212, 255, 0.25);"
        "    border-radius: 10px;"
        "}"
        "#firstRowFrame, #secondRowFrame, #thirdRowFrame, #fourthRowFrame, #fifthRowFrame {"
        "    background: transparent;"
        "}"
        // 圆形控制按钮：3D立体浮雕 + 深灰渐变 + 霓虹青蓝描边
        ".QPushButton[objectName=\"roundButton\"] {"
        "    width: 55px;"
        "    height: 55px;"
        "    border-radius: 27px;"
        "    border: 1.5px solid rgba(0, 212, 255, 0.75);"
        "    border-top: 1.5px solid rgba(140, 215, 245, 0.9);"
        "    border-left: 1.5px solid rgba(110, 200, 235, 0.85);"
        "    border-right: 1.5px solid rgba(0, 80, 110, 0.9);"
        "    border-bottom: 1.5px solid rgba(0, 55, 80, 0.95);"
        "    background: qradialgradient(cx:0.3, cy:0.25, radius:0.9, fx:0.3, fy:0.25, stop:0 rgba(82, 108, 138, 0.95), stop:0.4 rgba(42, 56, 72, 0.93), stop:0.8 rgba(20, 28, 38, 0.97), stop:1 rgba(6, 10, 16, 1));"
        "    color: #00d4ff;"
        "    font-size: 12px;"
        "    font-weight: bold;"
        "    text-align: center;"
        "}"
        ".QPushButton[objectName=\"roundButton\"]:hover {"
        "    border: 1.5px solid #00d4ff;"
        "    border-top: 1.5px solid rgba(180, 240, 255, 1);"
        "    border-left: 1.5px solid rgba(150, 225, 250, 0.95);"
        "    border-right: 1.5px solid rgba(0, 130, 170, 0.95);"
        "    border-bottom: 1.5px solid rgba(0, 95, 125, 1);"
        "    background: qradialgradient(cx:0.3, cy:0.25, radius:0.9, fx:0.3, fy:0.25, stop:0 rgba(102, 132, 168, 0.98), stop:0.4 rgba(52, 70, 90, 0.95), stop:0.8 rgba(26, 36, 48, 0.97), stop:1 rgba(10, 14, 20, 1));"
        "    color: #ffffff;"
        "}"
        ".QPushButton[objectName=\"roundButton\"]:pressed {"
        "    border-top: 1.5px solid rgba(0, 50, 70, 1);"
        "    border-left: 1.5px solid rgba(0, 65, 90, 0.95);"
        "    border-right: 1.5px solid rgba(100, 185, 215, 0.9);"
        "    border-bottom: 1.5px solid rgba(140, 215, 245, 0.95);"
        "    background: qradialgradient(cx:0.7, cy:0.75, radius:0.9, fx:0.7, fy:0.75, stop:0 rgba(50, 68, 88, 0.95), stop:0.5 rgba(22, 30, 40, 0.97), stop:1 rgba(4, 8, 12, 1));"
        "}"
        // 大圆形按钮（电源）：3D立体浮雕 + 更强霓虹发光
        ".QPushButton[objectName=\"largeRoundButton\"] {"
        "    width: 65px;"
        "    height: 65px;"
        "    border-radius: 32px;"
        "    border: 2px solid rgba(0, 212, 255, 0.8);"
        "    border-top: 2px solid rgba(140, 215, 245, 0.9);"
        "    border-left: 2px solid rgba(110, 200, 235, 0.85);"
        "    border-right: 2px solid rgba(0, 80, 110, 0.9);"
        "    border-bottom: 2px solid rgba(0, 55, 80, 0.95);"
        "    background: qradialgradient(cx:0.3, cy:0.25, radius:0.9, fx:0.3, fy:0.25, stop:0 rgba(82, 108, 138, 0.95), stop:0.4 rgba(42, 56, 72, 0.93), stop:0.8 rgba(20, 28, 38, 0.97), stop:1 rgba(6, 10, 16, 1));"
        "    color: #00d4ff;"
        "    font-size: 12px;"
        "    font-weight: bold;"
        "    text-align: center;"
        "}"
        ".QPushButton[objectName=\"largeRoundButton\"]:hover {"
        "    border: 2px solid #00d4ff;"
        "    border-top: 2px solid rgba(180, 240, 255, 1);"
        "    border-left: 2px solid rgba(150, 225, 250, 0.95);"
        "    border-right: 2px solid rgba(0, 130, 170, 0.95);"
        "    border-bottom: 2px solid rgba(0, 95, 125, 1);"
        "    background: qradialgradient(cx:0.3, cy:0.25, radius:0.9, fx:0.3, fy:0.25, stop:0 rgba(102, 132, 168, 0.98), stop:0.4 rgba(52, 70, 90, 0.95), stop:0.8 rgba(26, 36, 48, 0.97), stop:1 rgba(10, 14, 20, 1));"
        "    color: #ffffff;"
        "}"
        ".QPushButton[objectName=\"largeRoundButton\"]:pressed {"
        "    border-top: 2px solid rgba(0, 50, 70, 1);"
        "    border-left: 2px solid rgba(0, 65, 90, 0.95);"
        "    border-right: 2px solid rgba(100, 185, 215, 0.9);"
        "    border-bottom: 2px solid rgba(140, 215, 245, 0.95);"
        "    background: qradialgradient(cx:0.7, cy:0.75, radius:0.9, fx:0.7, fy:0.75, stop:0 rgba(50, 68, 88, 0.95), stop:0.5 rgba(22, 30, 40, 0.97), stop:1 rgba(4, 8, 12, 1));"
        "}"
        // 椭圆按钮：3D立体浮雕 + 玻璃拟态 + 霓虹描边
        ".QPushButton[objectName=\"ellipseButton\"] {"
        "    width: 65px;"
        "    height: 45px;"
        "    border-radius: 22px;"
        "    border: 1.5px solid rgba(0, 212, 255, 0.75);"
        "    border-top: 1.5px solid rgba(140, 215, 245, 0.9);"
        "    border-left: 1.5px solid rgba(110, 200, 235, 0.85);"
        "    border-right: 1.5px solid rgba(0, 80, 110, 0.9);"
        "    border-bottom: 1.5px solid rgba(0, 55, 80, 0.95);"
        "    background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 rgba(60, 80, 105, 0.95), stop:0.5 rgba(34, 46, 60, 0.92), stop:1 rgba(10, 14, 20, 0.98));"
        "    color: #00d4ff;"
        "    font-size: 13px;"
        "    font-weight: bold;"
        "    text-align: center;"
        "}"
        ".QPushButton[objectName=\"ellipseButton\"]:hover {"
        "    border: 1.5px solid #00d4ff;"
        "    border-top: 1.5px solid rgba(180, 240, 255, 1);"
        "    border-left: 1.5px solid rgba(150, 225, 250, 0.95);"
        "    border-right: 1.5px solid rgba(0, 130, 170, 0.95);"
        "    border-bottom: 1.5px solid rgba(0, 95, 125, 1);"
        "    background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 rgba(75, 100, 130, 0.98), stop:0.5 rgba(44, 60, 78, 0.95), stop:1 rgba(14, 20, 28, 1));"
        "    color: #ffffff;"
        "}"
        ".QPushButton[objectName=\"ellipseButton\"]:pressed {"
        "    border-top: 1.5px solid rgba(0, 50, 70, 1);"
        "    border-left: 1.5px solid rgba(0, 65, 90, 0.95);"
        "    border-right: 1.5px solid rgba(100, 185, 215, 0.9);"
        "    border-bottom: 1.5px solid rgba(140, 215, 245, 0.95);"
        "    background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 rgba(8, 12, 18, 1), stop:0.5 rgba(20, 28, 38, 0.97), stop:1 rgba(40, 54, 70, 0.95));"
        "}"
        // 小型快捷按钮（高度/速度）：3D立体浮雕 + 玻璃拟态
        ".QPushButton[objectName=\"smallBtn\"] {"
        "    padding: 5px 12px;"
        "    border-radius: 6px;"
        "    border: 1px solid rgba(0, 212, 255, 0.55);"
        "    border-top: 1px solid rgba(140, 215, 245, 0.85);"
        "    border-left: 1px solid rgba(110, 200, 235, 0.8);"
        "    border-right: 1px solid rgba(0, 80, 110, 0.85);"
        "    border-bottom: 1px solid rgba(0, 55, 80, 0.9);"
        "    background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 rgba(52, 70, 90, 0.95), stop:0.5 rgba(28, 38, 50, 0.9), stop:1 rgba(8, 12, 18, 0.98));"
        "    color: #00d4ff;"
        "    font-size: 12px;"
        "    font-weight: bold;"
        "}"
        ".QPushButton[objectName=\"smallBtn\"]:hover {"
        "    border: 1px solid #00d4ff;"
        "    border-top: 1px solid rgba(180, 240, 255, 1);"
        "    border-left: 1px solid rgba(150, 225, 250, 0.95);"
        "    border-right: 1px solid rgba(0, 130, 170, 0.95);"
        "    border-bottom: 1px solid rgba(0, 95, 125, 1);"
        "    background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 rgba(65, 88, 115, 0.98), stop:0.5 rgba(36, 50, 65, 0.95), stop:1 rgba(12, 16, 22, 1));"
        "    color: #ffffff;"
        "}"
        ".QPushButton[objectName=\"smallBtn\"]:pressed {"
        "    border-top: 1px solid rgba(0, 50, 70, 1);"
        "    border-left: 1px solid rgba(0, 65, 90, 0.95);"
        "    border-right: 1px solid rgba(100, 185, 215, 0.9);"
        "    border-bottom: 1px solid rgba(140, 215, 245, 0.95);"
        "    background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 rgba(6, 10, 16, 1), stop:1 rgba(34, 46, 60, 0.95));"
        "}"
        // 矩形按钮（能源更换）：3D立体浮雕
        ".QPushButton[objectName=\"rectButton\"] {"
        "    padding: 10px 24px;"
        "    border-radius: 8px;"
        "    border: 1.5px solid rgba(0, 212, 255, 0.75);"
        "    border-top: 1.5px solid rgba(140, 215, 245, 0.9);"
        "    border-left: 1.5px solid rgba(110, 200, 235, 0.85);"
        "    border-right: 1.5px solid rgba(0, 80, 110, 0.9);"
        "    border-bottom: 1.5px solid rgba(0, 55, 80, 0.95);"
        "    background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 rgba(60, 80, 105, 0.95), stop:0.5 rgba(34, 46, 60, 0.92), stop:1 rgba(10, 14, 20, 0.98));"
        "    color: #00d4ff;"
        "    font-size: 14px;"
        "    font-weight: bold;"
        "}"
        ".QPushButton[objectName=\"rectButton\"]:hover {"
        "    border: 1.5px solid #00d4ff;"
        "    border-top: 1.5px solid rgba(180, 240, 255, 1);"
        "    border-left: 1.5px solid rgba(150, 225, 250, 0.95);"
        "    border-right: 1.5px solid rgba(0, 130, 170, 0.95);"
        "    border-bottom: 1.5px solid rgba(0, 95, 125, 1);"
        "    background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 rgba(75, 100, 130, 0.98), stop:0.5 rgba(44, 60, 78, 0.95), stop:1 rgba(14, 20, 28, 1));"
        "    color: #ffffff;"
        "}"
        ".QPushButton[objectName=\"rectButton\"]:pressed {"
        "    border-top: 1.5px solid rgba(0, 50, 70, 1);"
        "    border-left: 1.5px solid rgba(0, 65, 90, 0.95);"
        "    border-right: 1.5px solid rgba(100, 185, 215, 0.9);"
        "    border-bottom: 1.5px solid rgba(140, 215, 245, 0.95);"
        "    background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 rgba(6, 10, 16, 1), stop:1 rgba(40, 54, 70, 0.95));"
        "}"
        // 小椭圆形按钮（近光/远光）：3D立体浮雕
        ".QPushButton[objectName=\"smallEllipseButton\"] {"
        "    width: 50px;"
        "    height: 35px;"
        "    border-radius: 17px;"
        "    border: 1.5px solid rgba(0, 212, 255, 0.65);"
        "    border-top: 1.5px solid rgba(140, 215, 245, 0.85);"
        "    border-left: 1.5px solid rgba(110, 200, 235, 0.8);"
        "    border-right: 1.5px solid rgba(0, 80, 110, 0.85);"
        "    border-bottom: 1.5px solid rgba(0, 55, 80, 0.9);"
        "    background: qradialgradient(cx:0.3, cy:0.25, radius:0.9, fx:0.3, fy:0.25, stop:0 rgba(72, 96, 124, 0.95), stop:0.5 rgba(32, 44, 58, 0.92), stop:1 rgba(6, 10, 16, 1));"
        "    color: #00d4ff;"
        "    font-size: 11px;"
        "    font-weight: bold;"
        "    text-align: center;"
        "}"
        // 进度条
        "QProgressBar {"
        "    border: 1px solid rgba(0, 212, 255, 0.4);"
        "    border-radius: 6px;"
        "    background: rgba(14, 20, 28, 0.85);"
        "    min-height: 14px;"
        "}"
        "QProgressBar::chunk {"
        "    background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #ff5252, stop:0.3 #ffd600, stop:0.6 #00e676, stop:1 #00e676);"
        "    border-radius: 5px;"
        "}"
        // 电池条
        "#batteryBar1, #batteryBar2, #batteryBar3, #batteryBar4, #batteryBar5, #batteryBar6 {"
        "    background: #00e676;"
        "    border-radius: 4px;"
        "    border: 1px solid rgba(0, 230, 118, 0.7);"
        "}";

    this->setStyleSheet(styleSheet);
}

/**
 * @brief 绘制事件
 * @details 在主界面背景上绘制科技感网格纹理和扫描线动画
 */
void Widget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false);

    int w = width();
    int h = height();

    // 1. 绘制细微网格纹理（低对比度，不干扰内容）
    painter.setPen(QPen(QColor(0, 212, 255, 18), 1, Qt::SolidLine));  // 极淡青蓝网格
    int gridSize = 40;
    for (int x = 0; x < w; x += gridSize) {
        painter.drawLine(x, 0, x, h);
    }
    for (int y = 0; y < h; y += gridSize) {
        painter.drawLine(0, y, w, y);
    }

    // 2. 绘制次级网格（更淡的小网格）
    painter.setPen(QPen(QColor(0, 212, 255, 8), 1, Qt::SolidLine));
    int subGridSize = 20;
    for (int x = 0; x < w; x += subGridSize) {
        if (x % gridSize != 0) {
            painter.drawLine(x, 0, x, h);
        }
    }
    for (int y = 0; y < h; y += subGridSize) {
        if (y % gridSize != 0) {
            painter.drawLine(0, y, w, y);
        }
    }

    // 3. 绘制装饰性边角标记（四角科技感L形标记）
    painter.setPen(QPen(QColor(0, 212, 255, 120), 2, Qt::SolidLine, Qt::SquareCap));
    int cornerSize = 20;
    int margin = 6;

    // 左上
    painter.drawLine(margin, margin, margin + cornerSize, margin);
    painter.drawLine(margin, margin, margin, margin + cornerSize);
    // 右上
    painter.drawLine(w - margin, margin, w - margin - cornerSize, margin);
    painter.drawLine(w - margin, margin, w - margin, margin + cornerSize);
    // 左下
    painter.drawLine(margin, h - margin, margin + cornerSize, h - margin);
    painter.drawLine(margin, h - margin, margin, h - margin - cornerSize);
    // 右下
    painter.drawLine(w - margin, h - margin, w - margin - cornerSize, h - margin);
    painter.drawLine(w - margin, h - margin, w - margin, h - margin - cornerSize);

    // 4. 绘制扫描线（从上往下移动，带渐变色拖尾）
    int scanY = m_scanLineY;
    if (scanY >= 0 && scanY < h) {
        // 扫描线核心（亮青蓝）
        QLinearGradient scanGrad(0, scanY - 30, 0, scanY + 30);
        scanGrad.setColorAt(0.0, QColor(0, 212, 255, 0));
        scanGrad.setColorAt(0.7, QColor(0, 212, 255, 30));
        scanGrad.setColorAt(0.95, QColor(0, 212, 255, 180));
        scanGrad.setColorAt(1.0, QColor(0, 212, 255, 220));
        painter.fillRect(0, scanY - 30, w, 60, QBrush(scanGrad));

        // 扫描线高光核心
        painter.setPen(QPen(QColor(100, 232, 255, 200), 1));
        painter.drawLine(0, scanY, w, scanY);

        // 扫描线顶部光晕
        QLinearGradient glowGrad(0, scanY - 5, 0, scanY + 5);
        glowGrad.setColorAt(0.0, QColor(0, 212, 255, 0));
        glowGrad.setColorAt(0.5, QColor(0, 212, 255, 60));
        glowGrad.setColorAt(1.0, QColor(0, 212, 255, 0));
        painter.fillRect(0, scanY - 5, w, 10, QBrush(glowGrad));
    }

    // 5. 绘制边缘光晕（左右两侧微弱发光）
    QLinearGradient leftGlow(0, 0, 30, 0);
    leftGlow.setColorAt(0.0, QColor(0, 212, 255, 25));
    leftGlow.setColorAt(1.0, QColor(0, 212, 255, 0));
    painter.fillRect(0, 0, 30, h, QBrush(leftGlow));

    QLinearGradient rightGlow(w - 30, 0, w, 0);
    rightGlow.setColorAt(0.0, QColor(0, 212, 255, 0));
    rightGlow.setColorAt(1.0, QColor(0, 212, 255, 25));
    painter.fillRect(w - 30, 0, 30, h, QBrush(rightGlow));
}

/**
 * @brief 为关键面板和控件设置霓虹发光效果
 * @details 使用QGraphicsDropShadowEffect为信息面板、标题栏等添加青蓝霓虹外发光
 */
void Widget::setupNeonGlowEffects()
{
    // 通过objectName查找关键面板并添加霓虹发光效果
    auto applyGlow = [](QWidget* widget, int blurRadius, int alpha) {
        if (!widget) return;
        QGraphicsDropShadowEffect *glow = new QGraphicsDropShadowEffect(widget);
        glow->setBlurRadius(blurRadius);
        glow->setColor(QColor(0, 212, 255, alpha));
        glow->setOffset(0, 0);
        widget->setGraphicsEffect(glow);
    };

    // 顶部标题栏 - 中等发光
    applyGlow(findChild<QFrame*>("topFrame"), 18, 85);

    // 所有信息面板（objectName="infoFrame"）- 霓虹青蓝发光 + 柔和高光
    QList<QFrame*> infoFrames = findChildren<QFrame*>("infoFrame");
    for (QFrame* frame : infoFrames) {
        applyGlow(frame, 16, 80);
    }

    // 电源按钮 - 强发光
    applyGlow(btnPower, 25, 120);

    // 指南针 - 柔和发光
    if (compassWidget) {
        applyGlow(compassWidget, 15, 70);
    }

    // 雷达 - 柔和发光
    if (radarWidget) {
        applyGlow(radarWidget, 15, 70);
    }
}

/**
 * @brief 扫描线动画定时器槽函数
 */
void Widget::onScanLineTimer()
{
    if (!m_scanLineDirDown) {
        m_scanLineY -= 2;
        if (m_scanLineY <= 0) {
            m_scanLineY = 0;
            m_scanLineDirDown = true;
        }
    } else {
        m_scanLineY += 2;
        if (m_scanLineY >= height()) {
            m_scanLineY = height();
            m_scanLineDirDown = false;
        }
    }
    update();  // 触发重绘
}

/**
 * @brief 鸣笛按钮按压事件处理
 * @details 实现鸣笛按压逻辑：按压时按钮变橙色立体效果
 */
void Widget::onHornPressed()
{
    if (!isPowerOn) {
        return;
    }
    btnHorn->setStyleSheet("width: 55px; height: 55px; border-radius: 27px; "
                           "border: 1.5px solid #ff8800; border-top: 1.5px solid #ffaa44; border-left: 1.5px solid #ff9922; border-right: 1.5px solid #aa5500; border-bottom: 1.5px solid #884400;"
                           "background: qradialgradient(cx:0.35, cy:0.3, radius:0.85, fx:0.35, fy:0.3, stop:0 rgba(255, 150, 50, 0.55), stop:0.6 rgba(180, 90, 0, 0.9), stop:1 rgba(100, 45, 0, 0.98)); "
                           "color: #ffaa00; font-size: 12px; font-weight: bold; text-align: center; "
                           "box-shadow: inset 0 1px 2px rgba(255,255,255,0.25), 0 0 20px rgba(255, 150, 50, 0.45), 0 3px 8px rgba(200,100,0,0.65);");
}

/**
 * @brief 鸣笛按钮释放事件处理
 * @details 实现鸣笛释放逻辑：释放后按钮恢复原始立体样式
 */
void Widget::onHornReleased()
{
    btnHorn->setStyleSheet("width: 55px; height: 55px; border-radius: 27px; "
                           "border: 1.5px solid rgba(0, 212, 255, 0.7); border-top: 1.5px solid rgba(140, 215, 245, 0.9); border-left: 1.5px solid rgba(110, 200, 235, 0.85); border-right: 1.5px solid rgba(0, 80, 110, 0.9); border-bottom: 1.5px solid rgba(0, 55, 80, 0.95);"
                           "background: qradialgradient(cx:0.3, cy:0.25, radius:0.9, fx:0.3, fy:0.25, stop:0 rgba(82, 108, 138, 0.95), stop:0.4 rgba(42, 56, 72, 0.93), stop:0.8 rgba(20, 28, 38, 0.97), stop:1 rgba(6, 10, 16, 1)); "
                           "color: #00d4ff; font-size: 12px; font-weight: bold; text-align: center; "
                           "box-shadow: inset 0 1px 2px rgba(0, 212, 255, 0.25), inset 0 -2px 4px rgba(0,0,0,0.5), 0 0 10px rgba(0, 212, 255, 0.15), 0 3px 8px rgba(0,0,0,0.65);");
}

/**
 * @brief 灯光按钮点击事件处理
 * @details 灯光按钮只负责开关：点击开启灯光（不附带近远光），再次点击关闭所有灯光
 */
void Widget::onLightsClicked()
{
    if (!isPowerOn) {
        // 如果电源未开启，不允许开灯
        return;  // 直接返回，不执行起飞操作
    }

    if (lightsState == 0) {
        // 灯光关闭状态，点击开启灯光（不附带近远光）
        lightsState = 3;  // 灯光开启但无近远光状态
        btnLights->setText("开启中");
        btnLights->setStyleSheet(lightsOnStyle);
        btnLowLight->setEnabled(true);
        btnHighLight->setEnabled(true);
    } else {
        // 灯光开启状态，点击关闭所有灯光
        lightsState = 0;
        btnLights->setText("关闭");
        btnLights->setStyleSheet(lightsOffStyle);
        btnLowLight->setStyleSheet(lowLightOffStyle);
        btnHighLight->setStyleSheet(highLightOffStyle);
        btnLowLight->setEnabled(false);
        btnHighLight->setEnabled(false);
    }
}

/**
 * @brief 近光按钮点击事件处理
 * @details 近光和远光互斥，近光开启时远光自动关闭
 */
void Widget::onLowLightClicked()
{
    if (btnLowLight->styleSheet() == lowLightOnStyle) {
        // 当前近光开启，关闭近光
        btnLowLight->setStyleSheet(lowLightOffStyle);
        // 如果远光也关闭，灯光按钮显示开启状态但无近远光
        if (btnHighLight->styleSheet() == highLightOffStyle) {
            lightsState = 3;  // 灯光开启但无近远光状态
            btnLights->setStyleSheet(lightsOnStyle);
        }
    } else {
        // 当前近光关闭，开启近光
        btnLowLight->setStyleSheet(lowLightOnStyle);
        btnHighLight->setStyleSheet(highLightOffStyle);  // 互斥：关闭远光
        lightsState = 1;  // 近光状态
        btnLights->setStyleSheet(lightsLowStyle);
    }
}

/**
 * @brief 远光按钮点击事件处理
 * @details 近光和远光互斥，远光开启时近光自动关闭
 */
void Widget::onHighLightClicked()
{
    if (btnHighLight->styleSheet() == highLightOnStyle) {
        // 当前远光开启，关闭远光
        btnHighLight->setStyleSheet(highLightOffStyle);
        // 如果近光也关闭，灯光按钮显示开启状态但无近远光
        if (btnLowLight->styleSheet() == lowLightOffStyle) {
            lightsState = 3;  // 灯光开启但无近远光状态
            btnLights->setStyleSheet(lightsOnStyle);
        }
    } else {
        // 当前远光关闭，开启远光
        btnHighLight->setStyleSheet(highLightOnStyle);
        btnLowLight->setStyleSheet(lowLightOffStyle);  // 互斥：关闭近光
        lightsState = 2;  // 远光状态
        btnLights->setStyleSheet(lightsHighStyle);
    }
}

/**
 * @brief 电源按钮点击事件处理
 * @details 实现电源开关逻辑：第一次点击开机刷新界面，第二次点击关机（需满足条件）
 */
void Widget::onPowerClicked()
{
    if (!isPowerOn) {
        // ==================== 第一次点击，开机 ====================
        isPowerOn = true;  // 将电源状态设置为开机

        // 将电源按钮样式改为绿色立体效果，表示开机状态（绿色状态色）
        btnPower->setStyleSheet("width: 55px; height: 55px; border-radius: 27px; "
                                "border: 1.5px solid #00e676; border-top: 1.5px solid #66ffaa; border-left: 1.5px solid #4dffa0; border-right: 1.5px solid #008040; border-bottom: 1.5px solid #006030;"
                                "background: qradialgradient(cx:0.35, cy:0.3, radius:0.85, fx:0.35, fy:0.3, stop:0 rgba(0, 230, 118, 0.5), stop:0.6 rgba(0, 130, 65, 0.9), stop:1 rgba(0, 60, 30, 0.98)); "
                                "color: #00e676; font-size: 12px; font-weight: bold; text-align: center; "
                                "box-shadow: inset 0 1px 2px rgba(255,255,255,0.25), 0 0 20px rgba(0, 230, 118, 0.45), 0 3px 8px rgba(0,150,70,0.65);");

        // ==================== 尝试连接 PX4 SITL ====================
        // 电源开启即尝试连接 PX4。连接成功后由 PX4 遥测驱动界面；
        // 连接失败则回退到原有模拟逻辑。
        if (px4Controller && !px4Connected) {
            qDebug() << "[Widget] 开机：尝试连接 PX4 SITL (udp://:14540)";
            px4Controller->connectToPx4(QStringLiteral("udp://:14540"));
        }

        // 加载当前地理环境的经纬度和电池电量数据
        // 先使用默认值，开机后通过GPS或IP定位获取真实位置
        latitude = 30.5728;    // 默认纬度（定位失败时使用）
        longitude = 104.0668;  // 默认经度（定位失败时使用）
        
        // 初始化6节独立电池：每节100%，当前使用第1节
        // 默认所有电池均已安装；第3、5节作为示例未安装（显示白色）
        for (int i = 0; i < 6; i++) {
            batteryPercentages[i] = 100;
            batteryInstalled[i] = true;
        }
        // 示例：第3节（索引2）和第5节（索引4）未安装
        batteryInstalled[2] = false;
        batteryInstalled[4] = false;
        batteryPercentages[2] = 0;
        batteryPercentages[4] = 0;
        currentBatteryIndex = 0;
        lowBatteryWarned = false;
        batteryLevel = getTotalBatteryLevel();
        
        positionSource = nullptr;
        gpsActive = false;
        usingGps = false;
        
        // 通过高德IP定位API获取真实位置
        requestRealLocation();
        
        // 更新界面显示地理环境数据
        lblCoordinates->setText("纬度: "+QString::number(latitude, 'f', 6) + "°N\n" + "经度: "+QString::number(longitude, 'f', 6) + "°E");
        updateBatteryDisplay();
        
        // 设置初始位置显示（不显示经纬度文字，显示地图）
        setPositionStatus("定位中...");                       // 显示等待提示
        
        // 其他数据保持为空/0（飞行高度、速度等），重量显示固定值
        lblFlightHeight->setText("高度: 0.00m");
        lblFlightSpeed->setText("速度: 0.00km/h");
        maxFlightSpeed = 10.0;                        // 重置最大速度为10km/h
        flightWeight = 58.5;      // 设置固定重量值
        lblWeight->setText("总载重: " + QString::number(40.00 + flightWeight, 'f', 2) + "kg");
        // ==================== 加载累计总行驶距离和总飞行时长（从QSettings持久化读取） ====================
        QSettings settings("DyqTech", "FlightController");
        totalDistance = settings.value("totalDistance", 0.0).toDouble();
        if (totalDistance < 0) totalDistance = 0.0;
        traveledDistance = 0.0;   // 开机时当前已行驶距离归零
        destinationDistance = 6.80;
        lblTotalDistance->setText("总行驶距离：" + QString::number(totalDistance, 'f', 2) + "km");
        lblTraveledDistance->setText("已行驶距离：" + QString::number(traveledDistance, 'f', 2) + "km");
        lblDistance->setText("目的地距离：" + QString::number(destinationDistance, 'f', 2) + "km");

        totalFlightSeconds = settings.value("totalFlightSeconds", 0).toInt();
        if (totalFlightSeconds < 0) totalFlightSeconds = 0;
        flightSeconds = 0;  // 开机时当前飞行时长归零
        // 更新总飞行时长显示
        int tHours = totalFlightSeconds / 3600;
        int tMinutes = (totalFlightSeconds % 3600) / 60;
        int tSeconds = totalFlightSeconds % 60;
        lblDurationValue->setText(QString("%1:%2:%3")
                                  .arg(tHours, 2, 10, QChar('0'))
                                  .arg(tMinutes, 2, 10, QChar('0'))
                                  .arg(tSeconds, 2, 10, QChar('0')));
        lblCurrentDurationValue->setText("00:00:00");  // 当前飞行时长归零显示
        
        // 启动地图位置更新定时器，每秒更新坐标文本
        mapUpdateTimer->start(1000);

        // 电源开启后，显示并同步顶层地图窗口（Tool + FramelessWindowHint）
        if (positionWebView && positionMapPlaceholder) {
            syncPositionMapGeometry();
            if (positionWebView->isHidden()) {
                positionWebView->show();
            }
            if (positionWebPage && positionMapReady) {
                // 地图已就绪则强制刷新一次渲染表面
                QTimer::singleShot(100, this, [this]() {
                    syncPositionMapGeometry();
                    positionWebPage->runJavaScript("_forceResize();");
                });
            }
        }
        
        // 电源开启后，高度和速度快捷按钮可以点击
        setShortcutButtonsEnabled(true);
    } else {
        // ==================== 第二次点击，关机 ====================
        // 关机安全检查：确保无人机不在飞行中，避免空中断电
        // 允许关机的条件：不在降落中，且无人机要么未解锁、要么实际高度低于 0.3m
        if (isLanding) {
            return;  // 正在降落中，不允许关机（等降落完成）
        }
        if (px4Armed && flightHeight > 0.3) {
            return;  // 无人机已解锁且高度 > 0.3m，真正在飞行中，不允许关机
        }

        // 满足关机条件，执行关机操作
        isPowerOn = false;       // 电源关闭
        isFlying = false;        // 重置飞行状态
        isHovering = false;      // 重置悬停状态
        isLanding = false;       // 重置降落状态

        // 关机时断开摄像头并停止视频显示，恢复占位文字
        stopVideoDisplay();
        
        // 将电源按钮样式恢复为默认的青蓝立体效果，表示关机状态
        btnPower->setStyleSheet("width: 55px; height: 55px; border-radius: 27px; "
                                "border: 1.5px solid rgba(0, 212, 255, 0.7); border-top: 1.5px solid rgba(140, 215, 245, 0.9); border-left: 1.5px solid rgba(110, 200, 235, 0.85); border-right: 1.5px solid rgba(0, 80, 110, 0.9); border-bottom: 1.5px solid rgba(0, 55, 80, 0.95);"
                                "background: qradialgradient(cx:0.3, cy:0.25, radius:0.9, fx:0.3, fy:0.25, stop:0 rgba(82, 108, 138, 0.95), stop:0.4 rgba(42, 56, 72, 0.93), stop:0.8 rgba(20, 28, 38, 0.97), stop:1 rgba(6, 10, 16, 1)); "
                                "color: #00d4ff; font-size: 12px; font-weight: bold; text-align: center; "
                                "box-shadow: inset 0 1px 2px rgba(0, 212, 255, 0.25), inset 0 -2px 4px rgba(0,0,0,0.5), 0 0 10px rgba(0, 212, 255, 0.15), 0 3px 8px rgba(0,0,0,0.65);");
        
        resetButtonStyles();  // 将所有按钮恢复到默认样式

        // 注意：时间更新定时器不随关机停止，保持时间显示持续刷新
        mapUpdateTimer->stop();   // 停止地图位置更新定时器

        // ==================== 关机时累计当前飞行时长到总飞行时长并持久化保存 ====================
        if (flightDurationTimer->isActive()) {
            flightDurationTimer->stop();  // 停止飞行时长定时器
        }
        totalFlightSeconds += flightSeconds;  // 当前时长累计到总时长
        flightSeconds = 0;                   // 重置当前飞行时长
        // 累计当前行驶距离到总行驶距离并持久化保存
        totalDistance += traveledDistance;
        traveledDistance = 0.0;
        QSettings settings("DyqTech", "FlightController");
        settings.setValue("totalFlightSeconds", totalFlightSeconds);  // 持久化保存总时长
        settings.setValue("totalDistance", totalDistance);  // 持久化保存总行驶距离
        // 更新总行驶距离显示
        lblTotalDistance->setText("总行驶距离：" + QString::number(totalDistance, 'f', 2) + "km");
        lblTraveledDistance->setText("已行驶距离：0.00km");
        // 更新总飞行时长显示
        int tHours = totalFlightSeconds / 3600;
        int tMinutes = (totalFlightSeconds % 3600) / 60;
        int tSeconds = totalFlightSeconds % 60;
        lblDurationValue->setText(QString("%1:%2:%3")
                                  .arg(tHours, 2, 10, QChar('0'))
                                  .arg(tMinutes, 2, 10, QChar('0'))
                                  .arg(tSeconds, 2, 10, QChar('0')));
        lblCurrentDurationValue->setText("00:00:00");  // 当前飞行时长归零显示

        // PX4 模式：关机时断开 PX4 连接
        if (px4Controller) {
            px4Controller->disconnect();
        }
        // 立即更新 PX4 状态标签
        if (lblPx4Status) {
            lblPx4Status->setText("PX4: 未连接");
            lblPx4Status->setStyleSheet(
                QString("QLabel#px4StatusLabel { "
                        "background: rgba(24, 30, 38, 0.9); "
                        "color: #ff5252; "
                        "border: 1px solid #ff5252; "
                        "border-radius: 6px; "
                        "padding: 4px 10px; "
                        "font-size: 11px; font-weight: bold; }"));
        }
        px4Connected = false;
        if (px4PollTimer) px4PollTimer->stop();
        if (dataUpdateTimer->isActive()) dataUpdateTimer->stop();

        // 电源关闭后，高度和速度快捷按钮不可点击
        setShortcutButtonsEnabled(false);

        // 电源关闭时隐藏地图顶层窗口
        if (positionWebView) {
            positionWebView->hide();
        }

        // 电源关闭时不清除数据，数据将由归还控件清除
    }
}

/**
 * @brief 起飞按钮点击事件处理
 * @details 实现起飞逻辑：点击后按钮变蓝色，启动数据更新定时器，显示实时数据窗口
 */
void Widget::onTakeoffClicked()
{
    // ==================== 检查前置条件 ====================
    if (!isPowerOn) {
        // 如果电源未开启，不允许起飞
        return;  // 直接返回，不执行起飞操作
    }
    if (isLanding) {
        // 如果飞机正在降落中，不允许起飞
        return;  // 直接返回，不执行起飞操作
    }
    
    // ==================== 重量判断 ====================
    bool wasNotFlying = !isFlying;
    double totalWeight = 40.00 + flightWeight;                 // 总载重 = 空重(40kg) + 当前实际重量
    if (wasNotFlying && totalWeight > 120.00) {
        // 超重无法起飞
        lblWeightValue->setText("限重120.00kg");
        lblWeightValue->setStyleSheet("color: #ff5252; font-size: 24px; font-weight: bold;");
        QMessageBox::warning(this, "警告", "超重无法起飞！当前总载重：" + QString::number(totalWeight, 'f', 2) + "kg，限重：120.00kg");
        return;
    }
    
    // ==================== 执行起飞操作 ====================
    isFlying = true;   // 将飞行状态设置为飞行中
    isHovering = false;  // 取消悬停状态，恢复正常数据更新

    // 如果是首次起飞（不在飞行状态），重置飞行数据
    if (wasNotFlying) {
        // 首次起飞：重置飞行数据（重量保持不变）
        flightHeight = 0.0;         // 初始飞行高度为0米，先起飞到目标高度
        targetFlightHeight = 0.6;   // 首次起飞目标高度为0.6m
        flightSpeed = 0.0;          // 初始飞行速度为0 km/h，到达目标高度后才开始加速
        batteryLevel = 85;           // 初始电池电量为85%
        // flightWeight 保持不变，使用开机时设置的固定值
        // 使用当前已定位的真实经纬度作为起飞点，不重置为固定值
        takeoffLatitude = latitude;   // 记录起飞时的纬度
        takeoffLongitude = longitude; // 记录起飞时的经度
        flightSeconds = 0;      // 重置当前飞行时长
        lblCurrentDurationValue->setText("00:00:00");  // 当前飞行时长归零显示
        // 注意：totalDistance不重置，保持累计总行驶距离
        traveledDistance = 0.0;  // 重置本次已行驶距离
        destinationDistance = 6.80;  // 重置目的地距离
        lblTotalDistance->setText("总行驶距离：" + QString::number(totalDistance, 'f', 2) + "km");
        lblTraveledDistance->setText("已行驶：" + QString::number(traveledDistance, 'f', 2) + "km");
        lblDistance->setText("目的地距离：" + QString::number(destinationDistance, 'f', 2) + "km");
        flightDurationTimer->start(1000);  // 启动飞行时长定时器
        mapUpdateTimer->start(1000);       // 启动地图位置更新定时器
        qDebug() << "[起飞] 起飞点: 纬度=" << takeoffLatitude << ", 经度=" << takeoffLongitude;
    } else {
        // 从悬停状态恢复起飞，保持当前高度和目标高度
        flightSpeed = 0.0;  // 速度重新从0开始加速
    }

    // 将起飞按钮样式改为蓝色立体效果，表示起飞状态（高对比蓝色）
    QString takeoffBlueStyle = "width: 55px; height: 55px; border-radius: 27px; "
                               "border: 1.5px solid #1a8cff; border-top: 1.5px solid #66b0ff; border-left: 1.5px solid #4da6ff; border-right: 1.5px solid #0050aa; border-bottom: 1.5px solid #003580;"
                               "background: qradialgradient(cx:0.35, cy:0.3, radius:0.85, fx:0.35, fy:0.3, stop:0 rgba(60, 140, 255, 0.55), stop:0.6 rgba(0, 80, 200, 0.9), stop:1 rgba(0, 35, 110, 0.98)); "
                               "color: #4da6ff; font-size: 12px; font-weight: bold; text-align: center; "
                               "box-shadow: inset 0 1px 2px rgba(255,255,255,0.25), 0 0 20px rgba(0, 150, 255, 0.45), 0 3px 8px rgba(0,80,200,0.65);";
    btnTakeoff->setStyleSheet(takeoffBlueStyle);
    btnTakeoff->style()->unpolish(btnTakeoff);
    btnTakeoff->style()->polish(btnTakeoff);

    // 恢复悬停和降落按钮的默认样式（互斥）
    QString defaultStyle = "width: 55px; height: 55px; border-radius: 27px; "
                           "border: 1.5px solid rgba(0, 212, 255, 0.7); border-top: 1.5px solid rgba(140, 215, 245, 0.9); border-left: 1.5px solid rgba(110, 200, 235, 0.85); border-right: 1.5px solid rgba(0, 80, 110, 0.9); border-bottom: 1.5px solid rgba(0, 55, 80, 0.95);"
                           "background: qradialgradient(cx:0.3, cy:0.25, radius:0.9, fx:0.3, fy:0.25, stop:0 rgba(82, 108, 138, 0.95), stop:0.4 rgba(42, 56, 72, 0.93), stop:0.8 rgba(20, 28, 38, 0.97), stop:1 rgba(6, 10, 16, 1)); "
                           "color: #00d4ff; font-size: 12px; font-weight: bold; text-align: center; "
                           "box-shadow: inset 0 1px 2px rgba(0, 212, 255, 0.25), inset 0 -2px 4px rgba(0,0,0,0.5), 0 0 10px rgba(0, 212, 255, 0.15), 0 3px 8px rgba(0,0,0,0.65);";
    btnHover->setStyleSheet(defaultStyle);
    btnHover->style()->unpolish(btnHover);
    btnHover->style()->polish(btnHover);
    btnLand->setStyleSheet(defaultStyle);
    btnLand->style()->unpolish(btnLand);
    btnLand->style()->polish(btnLand);

    // ==================== PX4 仿真模式：发送 armAndTakeoff 指令 ====================
    if (px4Connected && px4Controller) {  // PX4 已连接且控制器存在
        qDebug() << "[Widget] PX4 模式：发送 armAndTakeoff (高度=" << targetFlightHeight << "m)";  // 调试日志
        // 起飞高度最小 0.6m（之前为 2.5m，改为 0.6m 以支持低空起飞测试）
        double takeoffAlt = qMax(0.6, targetFlightHeight);           // 取目标高度与 0.6m 的较大值作为起飞高度
        px4Controller->armAndTakeoff(takeoffAlt);                    // 发送解锁+起飞指令到 PX4（异步执行）
        // PX4 模式下停止模拟数据定时器（避免模拟数据覆盖 PX4 真实遥测）
        if (dataUpdateTimer->isActive()) {                           // 如果模拟数据定时器正在运行
            dataUpdateTimer->stop();                                 // 停止它，防止模拟数据覆盖真实遥测
        }
        // 确保 PX4 遥测刷新定时器在运行（1Hz 刷新界面高度/速度等）
        if (px4PollTimer && !px4PollTimer->isActive()) {             // 如果 PX4 定时器未运行
            px4PollTimer->start();                                   // 启动它
        }
        setShortcutButtonsEnabled(true);                              // 启用高度/速度快捷按钮
        updateShortcutButtonHighlight();  // 起飞后根据当前数据更新快捷按钮高亮状态
        return;  // PX4 模式直接返回，不走模拟飞行逻辑
    }
    
    // 起飞时启用所有快捷按钮（高度和速度均可通过点击进行调整）
    btn1m5->setEnabled(true);       // 启用1.5米高度快捷按钮
    btn2m->setEnabled(true);        // 启用2.0米高度快捷按钮
    btn3m->setEnabled(true);        // 启用3.0米高度快捷按钮
    btn0m6->setEnabled(true);       // 启用0.6米高度快捷按钮
    btn1m0->setEnabled(true);       // 启用1.0米高度快捷按钮
    btn3km->setEnabled(true);       // 启用3 km/h速度快捷按钮
    btn5kmLow->setEnabled(true);    // 启用5 km/h速度快捷按钮
    btn10m->setEnabled(true);       // 启用10 km/h速度快捷按钮
    btn25km->setEnabled(true);      // 启用15 km/h速度快捷按钮
    btn1km->setEnabled(true);      // 启用25 km/h速度快捷按钮
    btn5km->setEnabled(true);       // 启用30 km/h速度快捷按钮

    updateShortcutButtonHighlight();  // 起飞后根据当前数据初始化快捷按钮高亮状态
    dataUpdateTimer->start(1000);  // 启动数据更新定时器，每秒更新一次飞行数据
}

/**
 * @brief 悬停按钮点击事件处理
 * @details 点击后弹出悬停模式选择对话框，包含三个按钮：
 *          盘旋悬停、前进悬停、后退悬停，默认选择前进悬停
 */
void Widget::onHoverClicked()
{
    // ==================== 检查前置条件 ====================
    if (!isPowerOn) {
        return;
    }
    if (!isFlying) {
        return;
    }

    // ==================== 如果对话框已存在，切换显示/隐藏 ====================
    if (hoverModeDialog && hoverModeDialog->isVisible()) {
        hoverModeDialog->hide();
        return;
    }

    // ==================== 创建悬停模式选择对话框（非模态） ====================
    hoverModeDialog = new QDialog(this);
    hoverModeDialog->setWindowTitle("选择悬停模式");
    hoverModeDialog->setFixedSize(340, 90);  // 缩小尺寸，只显示三个按钮
    hoverModeDialog->setWindowFlags(hoverModeDialog->windowFlags() | Qt::Tool);  // 工具窗口，任务栏不显示
    // 设置对话框科技风格样式（深灰黑磨砂 + 青蓝描边）
    hoverModeDialog->setStyleSheet(
        "QDialog { background: qlineargradient(x1:0, y1:0, x2:0, y2:1, "
        "stop:0 #1c2128, stop:1 #12161c); border: 1px solid rgba(0, 212, 255, 0.4); }"
        "QPushButton { min-width: 90px; min-height: 36px; border-radius: 8px; "
        "border: 1.5px solid rgba(0, 212, 255, 0.7); border-top: 1.5px solid rgba(140, 215, 245, 0.9); border-left: 1.5px solid rgba(110, 200, 235, 0.85); border-right: 1.5px solid rgba(0, 80, 110, 0.9); border-bottom: 1.5px solid rgba(0, 55, 80, 0.95);background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 rgba(44, 58, 74, 0.9), stop:1 rgba(18, 24, 32, 0.95)); "
        "color: #00d4ff; font-size: 13px; font-weight: bold; }"
        "QPushButton:hover { border: 1.5px solid #00d4ff; border-top: 1.5px solid #66e4ff; border-left: 1.5px solid #4ddaff; border-right: 1.5px solid #0080a0; border-bottom: 1.5px solid #006080;background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 rgba(56, 76, 98, 0.95), stop:1 rgba(24, 32, 42, 0.98)); color: #ffffff; }"
        "QPushButton:checked { background: qradialgradient(cx:0.35, cy:0.3, radius:0.85, fx:0.35, fy:0.3, "
        "stop:0 rgba(255, 82, 82, 0.55), stop:0.6 rgba(180, 30, 30, 0.9), stop:1 rgba(100, 15, 15, 0.98)); "
        "border: 1.5px solid #ff5252; border-top: 1.5px solid #ff9999; border-left: 1.5px solid #ff7777; border-right: 1.5px solid #aa2020; border-bottom: 1.5px solid #881515;color: #ff8a8a; }");

    QHBoxLayout *btnLayout = new QHBoxLayout(hoverModeDialog);
    btnLayout->setSpacing(10);
    btnLayout->setContentsMargins(10, 8, 10, 8);

    // 创建三个悬停模式按钮
    btnHoverCircle = new QPushButton("盘旋", hoverModeDialog);
    btnHoverStop = new QPushButton("悬停", hoverModeDialog);
    btnHoverBack = new QPushButton("后退", hoverModeDialog);

    btnHoverCircle->setCheckable(true);
    btnHoverStop->setCheckable(true);
    btnHoverBack->setCheckable(true);

    // 使用QButtonGroup实现互斥
    QButtonGroup *btnGroup = new QButtonGroup(hoverModeDialog);
    btnGroup->addButton(btnHoverCircle, 0);
    btnGroup->addButton(btnHoverStop, 1);
    btnGroup->addButton(btnHoverBack, 2);

    btnLayout->addWidget(btnHoverCircle);
    btnLayout->addWidget(btnHoverStop);
    btnLayout->addWidget(btnHoverBack);

    // 根据当前悬停模式设置默认选中状态
    if (hoverMode == CIRCLE_HOVER) {
        btnHoverCircle->setChecked(true);
    } else if (hoverMode == BACKWARD_HOVER) {
        btnHoverBack->setChecked(true);
    } else {
        btnHoverStop->setChecked(true);  // 默认选中悬停
    }

    // 点击按钮立即切换模式并生效
    connect(btnGroup, QOverload<int>::of(&QButtonGroup::buttonClicked), this, [this](int id) {
        switch (id) {
        case 0:  // 盘旋 - 显示旋转圆盘选择角度
            hoverMode = CIRCLE_HOVER;
            showRotationDial();
            break;
        case 1:  // 悬停（原地不动）
            hoverMode = FORWARD_HOVER;
            hideRotationDial();
            break;
        case 2:  // 后退
            hoverMode = BACKWARD_HOVER;
            hoverBackwardSeconds = 0;
            hideRotationDial();
            break;
        }

        // 立即执行悬停操作
        isHovering = true;
        isLanding = false;

        // PX4 模式：切换模式时也发送 Hold 指令（PX4 不区分盘旋/前进/后退，统一 Hold）
        if (px4Connected && px4Controller) {
            qDebug() << "[Widget] PX4 模式：模式切换时发送 hover (Hold)";
            px4Controller->hover();
            if (dataUpdateTimer->isActive()) {
                dataUpdateTimer->stop();
            }
        }

        // 更新主窗口悬停按钮为红色
        QString hoverRedStyle = "width: 55px; height: 55px; border-radius: 27px; "
                                "border: 1.5px solid #ff5252; border-top: 1.5px solid #ff9999; border-left: 1.5px solid #ff7777; border-right: 1.5px solid #aa2020; border-bottom: 1.5px solid #881515;"
                                "background: qradialgradient(cx:0.35, cy:0.3, radius:0.85, fx:0.35, fy:0.3, stop:0 rgba(255, 100, 100, 0.55), stop:0.6 rgba(180, 30, 30, 0.9), stop:1 rgba(100, 15, 15, 0.98)); "
                                "color: #ff5252; font-size: 12px; font-weight: bold; text-align: center; "
                                "box-shadow: inset 0 2px 4px rgba(255,255,255,0.25), 0 0 25px rgba(255, 100, 100, 0.5), 0 3px 8px rgba(150,0,0,0.7);";
        btnHover->setStyleSheet(hoverRedStyle);
        btnHover->style()->unpolish(btnHover);
        btnHover->style()->polish(btnHover);

        // 恢复起飞和降落按钮的默认样式
        QString defaultStyle = "width: 55px; height: 55px; border-radius: 27px; "
                               "border: 1.5px solid rgba(0, 212, 255, 0.7); border-top: 1.5px solid rgba(140, 215, 245, 0.9); border-left: 1.5px solid rgba(110, 200, 235, 0.85); border-right: 1.5px solid rgba(0, 80, 110, 0.9); border-bottom: 1.5px solid rgba(0, 55, 80, 0.95);"
                               "background: qradialgradient(cx:0.3, cy:0.25, radius:0.9, fx:0.3, fy:0.25, stop:0 rgba(82, 108, 138, 0.95), stop:0.4 rgba(42, 56, 72, 0.93), stop:0.8 rgba(20, 28, 38, 0.97), stop:1 rgba(6, 10, 16, 1)); "
                               "color: #00d4ff; font-size: 12px; font-weight: bold; text-align: center; "
                               "box-shadow: inset 0 1px 2px rgba(0, 212, 255, 0.25), inset 0 -2px 4px rgba(0,0,0,0.5), 0 0 10px rgba(0, 212, 255, 0.15), 0 3px 8px rgba(0,0,0,0.65);";
        btnTakeoff->setStyleSheet(defaultStyle);
        btnTakeoff->style()->unpolish(btnTakeoff);
        btnTakeoff->style()->polish(btnTakeoff);
        btnLand->setStyleSheet(defaultStyle);
        btnLand->style()->unpolish(btnLand);
        btnLand->style()->polish(btnLand);

        setShortcutButtonsEnabled(true);
    });

    // 显示非模态对话框
    hoverModeDialog->show();
    hoverModeDialog->raise();
    hoverModeDialog->activateWindow();

    // 立即执行悬停操作（默认选中的模式）
    isHovering = true;
    isLanding = false;

    // ==================== PX4 仿真模式：发送 Hold 悬停指令 ====================
    if (px4Connected && px4Controller) {
        qDebug() << "[Widget] PX4 模式：发送 hover (Hold)";
        px4Controller->hover();
        // PX4 模式下停止模拟数据定时器
        if (dataUpdateTimer->isActive()) {
            dataUpdateTimer->stop();
        }
    }

    // 更新主窗口悬停按钮为红色
    QString hoverRedStyle = "width: 55px; height: 55px; border-radius: 27px; "
                            "border: 1.5px solid #ff5252; border-top: 1.5px solid #ff9999; border-left: 1.5px solid #ff7777; border-right: 1.5px solid #aa2020; border-bottom: 1.5px solid #881515;"
                            "background: qradialgradient(cx:0.35, cy:0.3, radius:0.85, fx:0.35, fy:0.3, stop:0 rgba(255, 100, 100, 0.55), stop:0.6 rgba(180, 30, 30, 0.9), stop:1 rgba(100, 15, 15, 0.98)); "
                            "color: #ff5252; font-size: 12px; font-weight: bold; text-align: center; "
                            "box-shadow: inset 0 2px 4px rgba(255,255,255,0.25), 0 0 25px rgba(255, 100, 100, 0.5), 0 3px 8px rgba(150,0,0,0.7);";
    btnHover->setStyleSheet(hoverRedStyle);
    btnHover->style()->unpolish(btnHover);
    btnHover->style()->polish(btnHover);

    // 恢复起飞和降落按钮的默认样式
    QString defaultStyle = "width: 55px; height: 55px; border-radius: 27px; "
                           "border: 1.5px solid rgba(0, 212, 255, 0.7); border-top: 1.5px solid rgba(140, 215, 245, 0.9); border-left: 1.5px solid rgba(110, 200, 235, 0.85); border-right: 1.5px solid rgba(0, 80, 110, 0.9); border-bottom: 1.5px solid rgba(0, 55, 80, 0.95);"
                           "background: qradialgradient(cx:0.3, cy:0.25, radius:0.9, fx:0.3, fy:0.25, stop:0 rgba(82, 108, 138, 0.95), stop:0.4 rgba(42, 56, 72, 0.93), stop:0.8 rgba(20, 28, 38, 0.97), stop:1 rgba(6, 10, 16, 1)); "
                           "color: #00d4ff; font-size: 12px; font-weight: bold; text-align: center; "
                           "box-shadow: inset 0 1px 2px rgba(0, 212, 255, 0.25), inset 0 -2px 4px rgba(0,0,0,0.5), 0 0 10px rgba(0, 212, 255, 0.15), 0 3px 8px rgba(0,0,0,0.65);";
    btnTakeoff->setStyleSheet(defaultStyle);
    btnTakeoff->style()->unpolish(btnTakeoff);
    btnTakeoff->style()->polish(btnTakeoff);
    btnLand->setStyleSheet(defaultStyle);
    btnLand->style()->unpolish(btnLand);
    btnLand->style()->polish(btnLand);

    setShortcutButtonsEnabled(true);
}

/**
 * @brief 显示旋转圆盘选择对话框
 */
void Widget::showRotationDial()
{
    if (rotationDialDialog && rotationDialDialog->isVisible()) {
        rotationDialDialog->raise();
        rotationDialDialog->activateWindow();
        return;
    }

    // 创建旋转圆盘对话框
    rotationDialDialog = new QDialog(this);
    rotationDialDialog->setWindowTitle("选择旋转角度");
    rotationDialDialog->setFixedSize(220, 240);
    rotationDialDialog->setWindowFlags(rotationDialDialog->windowFlags() | Qt::Tool);
    rotationDialDialog->setStyleSheet(
        "QDialog { background: qlineargradient(x1:0, y1:0, x2:0, y2:1, "
        "stop:0 #1c2128, stop:1 #0f1217); border: 1px solid rgba(0, 212, 255, 0.5); }");

    QVBoxLayout *layout = new QVBoxLayout(rotationDialDialog);
    layout->setSpacing(5);
    layout->setContentsMargins(5, 5, 5, 5);

    // 旋转圆盘控件（点击扇形立即生效）
    rotationDial = new RotationDial(rotationDialDialog);
    layout->addWidget(rotationDial, 0, Qt::AlignCenter);

    // 扇形点击信号 - 直接开始旋转
    connect(rotationDial, &RotationDial::sectorClicked, this, [this](int angle) {
        if (angle > 0) {
            startRotation(angle);
        }
    });

    rotationDialDialog->show();
    rotationDialDialog->raise();
    rotationDialDialog->activateWindow();
}

/**
 * @brief 隐藏旋转圆盘选择对话框
 */
void Widget::hideRotationDial()
{
    if (rotationDialDialog) {
        rotationDialDialog->hide();
    }
    // 停止旋转
    isRotating = false;
}

/**
 * @brief 开始旋转
 * @param angle 目标旋转角度（90/180/270/360）
 */
void Widget::startRotation(int angle)
{
    rotationTargetAngle = angle;
    rotationCurrentAngle = 0;
    isRotating = true;

    // 设置悬停状态
    isHovering = true;
    isLanding = false;
    hoverMode = CIRCLE_HOVER;

    // 飞行器以1km/h速度旋转
    flightSpeed = 1.0;

    // 关闭旋转圆盘对话框
    hideRotationDial();

    // 隐藏悬停模式对话框
    if (hoverModeDialog) {
        hoverModeDialog->hide();
    }

    // 更新主窗口悬停按钮为红色
    QString hoverRedStyle = "width: 55px; height: 55px; border-radius: 27px; "
                            "border: 1.5px solid #ff5252; border-top: 1.5px solid #ff9999; border-left: 1.5px solid #ff7777; border-right: 1.5px solid #aa2020; border-bottom: 1.5px solid #881515;"
                            "background: qradialgradient(cx:0.35, cy:0.3, radius:0.85, fx:0.35, fy:0.3, stop:0 rgba(255, 100, 100, 0.55), stop:0.6 rgba(180, 30, 30, 0.9), stop:1 rgba(100, 15, 15, 0.98)); "
                            "color: #ff5252; font-size: 12px; font-weight: bold; text-align: center; "
                            "box-shadow: inset 0 2px 4px rgba(255,255,255,0.25), 0 0 25px rgba(255, 100, 100, 0.5), 0 3px 8px rgba(150,0,0,0.7);";
    btnHover->setStyleSheet(hoverRedStyle);
    btnHover->style()->unpolish(btnHover);
    btnHover->style()->polish(btnHover);

    // 恢复起飞和降落按钮的默认样式
    QString defaultStyle = "width: 55px; height: 55px; border-radius: 27px; "
                           "border: 1.5px solid rgba(0, 212, 255, 0.7); border-top: 1.5px solid rgba(140, 215, 245, 0.9); border-left: 1.5px solid rgba(110, 200, 235, 0.85); border-right: 1.5px solid rgba(0, 80, 110, 0.9); border-bottom: 1.5px solid rgba(0, 55, 80, 0.95);"
                           "background: qradialgradient(cx:0.3, cy:0.25, radius:0.9, fx:0.3, fy:0.25, stop:0 rgba(82, 108, 138, 0.95), stop:0.4 rgba(42, 56, 72, 0.93), stop:0.8 rgba(20, 28, 38, 0.97), stop:1 rgba(6, 10, 16, 1)); "
                           "color: #00d4ff; font-size: 12px; font-weight: bold; text-align: center; "
                           "box-shadow: inset 0 1px 2px rgba(0, 212, 255, 0.25), inset 0 -2px 4px rgba(0,0,0,0.5), 0 0 10px rgba(0, 212, 255, 0.15), 0 3px 8px rgba(0,0,0,0.65);";
    btnTakeoff->setStyleSheet(defaultStyle);
    btnTakeoff->style()->unpolish(btnTakeoff);
    btnTakeoff->style()->polish(btnTakeoff);
    btnLand->setStyleSheet(defaultStyle);
    btnLand->style()->unpolish(btnLand);
    btnLand->style()->polish(btnLand);

    setShortcutButtonsEnabled(true);
}

/**
 * @brief 降落按钮点击事件处理
 * @details 实现降落逻辑：点击后开始降落，3秒后停稳，数据更新停止，起飞按钮恢复默认样式
 */
void Widget::onLandClicked()
{
    // ==================== 检查前置条件 ====================
    if (!isPowerOn) {
        // 如果电源未开启，不允许降落
        return;  // 直接返回，不执行降落操作
    }
    if (!isFlying) {
        // 如果飞机未起飞，不允许降落
        return;  // 直接返回，不执行降落操作
    }
    if (isLanding) {
        // 如果飞机正在降落中，不允许重复降落
        return;  // 直接返回，不执行降落操作
    }
    
    // ==================== 执行降落操作 ====================
    isLanding = true;   // 将降落状态设置为正在降落
    isHovering = false;  // 取消悬停状态

    // 视频显示仅在点击降落时启动：自动连接 USB 摄像头并将画面显示到视频显示窗口
    startVideoDisplay();

    // 将降落按钮样式改为黄色立体效果，表示正在降落（黄色警示色）
    QString landYellowStyle = "width: 55px; height: 55px; border-radius: 27px; "
                               "border: 1.5px solid #ffd600; border-top: 1.5px solid #fff066; border-left: 1.5px solid #ffe633; border-right: 1.5px solid #aa8c00; border-bottom: 1.5px solid #886b00;"
                               "background: qradialgradient(cx:0.35, cy:0.3, radius:0.85, fx:0.35, fy:0.3, stop:0 rgba(255, 214, 0, 0.5), stop:0.6 rgba(180, 150, 10, 0.9), stop:1 rgba(100, 80, 5, 0.98)); "
                               "color: #ffd600; font-size: 12px; font-weight: bold; text-align: center; "
                               "box-shadow: inset 0 1px 2px rgba(255,255,255,0.3), 0 0 20px rgba(255, 214, 0, 0.45), 0 3px 8px rgba(200,150,0,0.65);";
    btnLand->setStyleSheet(landYellowStyle);
    btnLand->style()->unpolish(btnLand);
    btnLand->style()->polish(btnLand);

    // ==================== PX4 仿真模式：发送 land 指令 ====================
    if (px4Connected && px4Controller) {
        qDebug() << "[Widget] PX4 模式：发送 land";
        px4Controller->land();

        // PX4 模式下停止模拟数据定时器
        if (dataUpdateTimer->isActive()) {
            dataUpdateTimer->stop();
        }

        // 恢复悬停和起飞按钮为默认样式
        QString defaultStyle = "width: 55px; height: 55px; border-radius: 27px; "
                               "border: 1.5px solid rgba(0, 212, 255, 0.7); border-top: 1.5px solid rgba(140, 215, 245, 0.9); border-left: 1.5px solid rgba(110, 200, 235, 0.85); border-right: 1.5px solid rgba(0, 80, 110, 0.9); border-bottom: 1.5px solid rgba(0, 55, 80, 0.95);"
                               "background: qradialgradient(cx:0.3, cy:0.25, radius:0.9, fx:0.3, fy:0.25, stop:0 rgba(82, 108, 138, 0.95), stop:0.4 rgba(42, 56, 72, 0.93), stop:0.8 rgba(20, 28, 38, 0.97), stop:1 rgba(6, 10, 16, 1)); "
                               "color: #00d4ff; font-size: 12px; font-weight: bold; text-align: center; "
                               "box-shadow: inset 0 1px 2px rgba(0, 212, 255, 0.25), inset 0 -2px 4px rgba(0,0,0,0.5), 0 0 10px rgba(0, 212, 255, 0.15), 0 3px 8px rgba(0,0,0,0.65);";
        btnHover->setStyleSheet(defaultStyle);
        btnHover->style()->unpolish(btnHover);
        btnHover->style()->polish(btnHover);
        btnTakeoff->setStyleSheet(defaultStyle);
        btnTakeoff->style()->unpolish(btnTakeoff);
        btnTakeoff->style()->polish(btnTakeoff);

        setShortcutButtonsEnabled(true);
        return;
    }

    // 模拟模式：启动数据更新定时器（如果之前因为悬停停止了更新）
    if (!dataUpdateTimer->isActive()) {
        dataUpdateTimer->start(1000);
    }

    // 恢复起飞和悬停按钮的默认样式（互斥）
    QString defaultStyle = "width: 55px; height: 55px; border-radius: 27px; "
                           "border: 1.5px solid rgba(0, 212, 255, 0.7); border-top: 1.5px solid rgba(140, 215, 245, 0.9); border-left: 1.5px solid rgba(110, 200, 235, 0.85); border-right: 1.5px solid rgba(0, 80, 110, 0.9); border-bottom: 1.5px solid rgba(0, 55, 80, 0.95);"
                           "background: qradialgradient(cx:0.3, cy:0.25, radius:0.9, fx:0.3, fy:0.25, stop:0 rgba(82, 108, 138, 0.95), stop:0.4 rgba(42, 56, 72, 0.93), stop:0.8 rgba(20, 28, 38, 0.97), stop:1 rgba(6, 10, 16, 1)); "
                           "color: #00d4ff; font-size: 12px; font-weight: bold; text-align: center; "
                           "box-shadow: inset 0 1px 2px rgba(0, 212, 255, 0.25), inset 0 -2px 4px rgba(0,0,0,0.5), 0 0 10px rgba(0, 212, 255, 0.15), 0 3px 8px rgba(0,0,0,0.65);";
    btnTakeoff->setStyleSheet(defaultStyle);
    btnTakeoff->style()->unpolish(btnTakeoff);
    btnTakeoff->style()->polish(btnTakeoff);
    btnHover->setStyleSheet(defaultStyle);
    btnHover->style()->unpolish(btnHover);
    btnHover->style()->polish(btnHover);
    
    // 降落时禁用高度和速度快捷按钮
    setShortcutButtonsEnabled(false);
}

/**
 * @brief 启动视频显示（连接摄像头并显示画面）
 * @details 在点击"降落"时调用：
 *          1. 若摄像头已在采集，直接返回避免重复打开
 *          2. 枚举摄像头索引 0~9，逐个尝试打开并读取测试帧，
 *             选中第一个能真正采集画面的设备（含笔记本自带摄像头、USB 摄像头）。
 *             通过测试帧过滤掉 V4L2 元数据设备（能 open 但 read 空帧）。
 *          3. 找到可用设备后启动约 30 FPS 的帧采集定时器刷新视频显示窗口；
 *             全部不可用则在窗口显示"摄像头未连接"提示。
 */
void Widget::startVideoDisplay()
{
    // 已在采集则不重复打开（避免重复占用摄像头设备）
    if (videoCapture && videoCapture->isOpened()) {
        return;
    }

    // 释放可能残留的旧 capture 对象
    if (videoCapture) {
        videoCapture->release();
        delete videoCapture;
        videoCapture = nullptr;
    }

    // 枚举摄像头索引 0~9，找到第一个能真正采集画面的设备
    int selectedIndex = -1;  // 选中的可用设备索引
    for (int idx = 0; idx < 10; ++idx) {
        cv::VideoCapture *probe = new cv::VideoCapture(idx);
        if (!probe->isOpened()) {
            // 该索引打不开，尝试下一个
            delete probe;
            continue;
        }
        // 读取测试帧：过滤 V4L2 元数据设备（能 open 但 read 返回空帧）
        cv::Mat testFrame;
        bool ok = false;
        // 多读几帧：某些摄像头首帧未就绪
        for (int retry = 0; retry < 5; ++retry) {
            if (probe->read(testFrame) && !testFrame.empty()) {
                ok = true;
                break;
            }
        }
        if (ok) {
            // 找到可用设备，保留该对象作为采集源
            selectedIndex = idx;
            videoCapture = probe;
            break;
        }
        // 该设备读不出帧，释放并尝试下一个索引
        probe->release();
        delete probe;
    }

    if (selectedIndex < 0) {
        // 所有索引均不可用：在视频窗口显示提示
        qDebug() << "[Widget] 视频显示：未找到可用摄像头设备（0~9 均不可用）";
        videoDisplayLabel->clear();
        videoDisplayLabel->setText("摄像头未连接");
        return;
    }

    qDebug() << "[Widget] 视频显示：摄像头已连接（索引" << selectedIndex << "），启动画面采集（~30 FPS）";

    // 创建并启动帧采集定时器（33ms ≈ 30 FPS）
    if (!videoTimer) {
        videoTimer = new QTimer(this);
        connect(videoTimer, &QTimer::timeout, this, &Widget::onVideoFrameReady);
    }
    videoTimer->start(33);
}

/**
 * @brief 停止视频显示（断开摄像头并恢复占位）
 * @details 关机或析构时调用：停止采集定时器、释放摄像头资源，
 *          并将视频显示窗口恢复为"视频显示窗口"占位文字。
 */
void Widget::stopVideoDisplay()
{
    // 停止帧采集定时器
    if (videoTimer) {
        videoTimer->stop();
    }

    // 释放摄像头资源
    if (videoCapture) {
        if (videoCapture->isOpened()) {
            videoCapture->release();
        }
        delete videoCapture;
        videoCapture = nullptr;
        qDebug() << "[Widget] 视频显示：摄像头已断开";
    }

    // 恢复占位文字
    if (videoDisplayLabel) {
        videoDisplayLabel->clear();
        videoDisplayLabel->setText("视频显示窗口");
    }
}

/**
 * @brief 视频帧采集定时器超时槽函数
 * @details 从 OpenCV VideoCapture 读取一帧（BGR 格式），转为 RGB 后构造 QImage，
 *          按视频显示窗口当前尺寸等比缩放（保持宽高比）并显示。
 *          摄像头未就绪、读取空帧或窗口尺寸无效时跳过本次刷新。
 */
void Widget::onVideoFrameReady()
{
    // 前置检查：摄像头对象与打开状态
    if (!videoCapture || !videoCapture->isOpened()) {
        return;
    }
    // 窗口尺寸有效性检查（避免缩放到 0 产生空 pixmap）
    if (videoDisplayLabel->width() <= 0 || videoDisplayLabel->height() <= 0) {
        return;
    }

    cv::Mat frame;  // BGR 帧
    if (!videoCapture->read(frame) || frame.empty()) {
        return;  // 读取失败或空帧，跳过本次刷新
    }

    // BGR -> RGB 转换，匹配 QImage::Format_RGB888
    cv::Mat rgb;
    cv::cvtColor(frame, rgb, cv::COLOR_BGR2RGB);

    // 构造 QImage（copy 深拷贝数据，避免 rgb 局部变量释放后悬空指针）
    QImage img(rgb.data, rgb.cols, rgb.rows,
               static_cast<int>(rgb.step), QImage::Format_RGB888);

    // 等比缩放至视频显示窗口尺寸并显示
    videoDisplayLabel->setPixmap(
        QPixmap::fromImage(img.copy()).scaled(
            videoDisplayLabel->size(),
            Qt::KeepAspectRatio,
            Qt::SmoothTransformation));
}

/**
 * @brief 指挥中心按钮点击事件处理
 * @details 实现指挥中心连接逻辑：点击后按钮变绿色，表示已连接指挥中心
 */
void Widget::onCommandCenterClicked()
{
    // ==================== 检查前置条件 ====================
    if (!isPowerOn) {
        // 如果电源未开启，不允许连接指挥中心
        QMessageBox::warning(this, "警告", "请先开启电源");
        return;  // 直接返回，不执行连接操作
    }
    
    // ==================== 执行连接指挥中心操作 ====================
    // 将指挥中心按钮样式改为青蓝高亮，表示已连接状态（状态指示色）
    btnCommandCenter->setStyleSheet("width: 55px; height: 55px; border-radius: 27px; "
                                    "border: 1.5px solid #00d4ff; border-top: 1.5px solid #66e4ff; border-left: 1.5px solid #4ddaff; border-right: 1.5px solid #0080a0; border-bottom: 1.5px solid #006080;"
                                    "background: qradialgradient(cx:0.35, cy:0.3, radius:0.85, fx:0.35, fy:0.3, stop:0 rgba(0, 212, 255, 0.5), stop:0.6 rgba(0, 100, 150, 0.9), stop:1 rgba(0, 50, 80, 0.98)); "
                                    "color: #ffffff; font-size: 11px; font-weight: bold; text-align: center; "
                                    "box-shadow: inset 0 1px 2px rgba(255,255,255,0.3), 0 0 18px rgba(0, 212, 255, 0.5), 0 3px 8px rgba(0,120,180,0.65);");
    
    QMessageBox::information(this, "提示", "已连接指挥中心");  // 弹出提示信息
}

/**
 * @brief 结束按钮点击事件处理
 * @details 实现结束逻辑：点击后按钮变紫红色，清除飞行数据，5秒后恢复原始状态并提示结束成功
 */
void Widget::onReturnClicked()
{
    // ==================== 检查前置条件 ====================
    if (isPowerOn) {
        // 如果电源已开启，不允许结束
        QMessageBox::warning(this, "警告", "请先关闭电源");
        return;  // 直接返回，不执行结束操作
    }

    // ==================== 执行结束操作 ====================
    // 将结束按钮样式改为红色警示立体效果，表示结束中（红色警示色）
    QString returnPurpleStyle = "width: 55px; height: 55px; border-radius: 27px; "
                                "border: 1.5px solid #ff5252; border-top: 1.5px solid #ff9999; border-left: 1.5px solid #ff7777; border-right: 1.5px solid #aa2020; border-bottom: 1.5px solid #881515;"
                                "background: qradialgradient(cx:0.35, cy:0.3, radius:0.85, fx:0.35, fy:0.3, stop:0 rgba(255, 82, 82, 0.5), stop:0.6 rgba(180, 30, 30, 0.9), stop:1 rgba(100, 15, 15, 0.98)); "
                                "color: #ff8a8a; font-size: 12px; font-weight: bold; text-align: center; "
                                "box-shadow: inset 0 1px 2px rgba(255,255,255,0.25), 0 0 18px rgba(255, 82, 82, 0.45), 0 3px 8px rgba(150,0,0,0.65);";
    btnReturn->setStyleSheet(returnPurpleStyle);
    btnReturn->style()->unpolish(btnReturn);
    btnReturn->style()->polish(btnReturn);
    
    // ==================== 清除飞行数据 ====================
    // 清除当前飞行时长（总飞行时长保持不变，关机时才累计）
    flightSeconds = 0;
    lblCurrentDurationValue->setText("00:00:00");
    
    // 清除经纬度
    latitude = 0.0;
    longitude = 0.0;
    lblCoordinates->setText("纬度: 0.000000°N\n经度: 0.000000°E");
    
    // 清除目的地距离
    destinationDistance = 0.0;
    // 注意：totalDistance不重置，保持累计总行驶距离
    traveledDistance = 0.0;
    lblTotalDistance->setText("总行驶距离：" + QString::number(totalDistance, 'f', 2) + "km");
    lblTraveledDistance->setText("已行驶距离：0.00km");
    lblDistance->setText("目的地距离：0.00km");
    
    // 清除电量显示
    for (int i = 0; i < 6; i++) {
        batteryPercentages[i] = 0;
        batteryInstalled[i] = false;  // 关机时所有电池标记为未安装
    }
    currentBatteryIndex = 0;
    lowBatteryWarned = false;
    batteryLevel = 0;
    updateBatteryDisplay();
    
    // 清除当前位置信息框状态显示
    setPositionStatus("");
    
    // 清除最大速度显示
    maxFlightSpeed = 0.0;

    // ==================== 重置高度和速度快捷按钮状态 ====================
    setShortcutButtonsEnabled(false);                          // 禁用所有快捷按钮（不可点击）
    clearShortcutButtonHighlight();                            // 清除所有快捷按钮的高亮状态（恢复未点击外观）

    // 启动5秒后恢复状态的定时器
    returnResetTimer->start();
}

/**
 * @brief 能源更换按钮点击事件处理
 * @details 实现能源更换逻辑：点击后按钮变黄色，表示能源更换中，弹窗关闭后恢复原始样式
 */
void Widget::onEnergyReplaceClicked()
{
    // ==================== 检查前置条件 ====================
    if (isPowerOn) {
        // 如果电源未开启，不允许更换能源
        QMessageBox::warning(this, "警告", "请先关闭电源");
        return;  // 直接返回，不执行更换操作
    }
    
    // ==================== 执行能源更换操作 ====================
    // 将能源更换按钮样式改为黄色警示立体效果，表示能源更换中（黄色警示色）
    btnEnergyReplace->setStyleSheet("padding: 12px 30px; border-radius: 6px; "
                                    "border: 1.5px solid #ffd600; border-top: 1.5px solid #fff066; border-left: 1.5px solid #ffe633; border-right: 1.5px solid #aa8c00; border-bottom: 1.5px solid #886b00;"
                                    "background: qradialgradient(cx:0.35, cy:0.3, radius:0.85, fx:0.35, fy:0.3, stop:0 rgba(255, 214, 0, 0.5), stop:0.6 rgba(180, 150, 10, 0.9), stop:1 rgba(100, 80, 5, 0.98)); "
                                    "color: #ffd600; font-size: 14px; font-weight: bold; "
                                    "box-shadow: inset 0 1px 2px rgba(255,255,255,0.25), 0 0 18px rgba(255, 214, 0, 0.4), 0 3px 8px rgba(200,150,0,0.6);");
    
    QMessageBox::information(this, "提示", "能源更换完成");  // 弹出提示信息
    
    // 能源更换完成后：所有电池重新安装并充满
    for (int i = 0; i < 6; i++) {
        batteryPercentages[i] = 100;
        batteryInstalled[i] = true;
    }
    currentBatteryIndex = 0;
    lowBatteryWarned = false;
    batteryLevel = getTotalBatteryLevel();
    updateBatteryDisplay();
    
    // 弹窗关闭后恢复按钮原始立体样式（磨砂深灰 + 青蓝描边）
    btnEnergyReplace->setStyleSheet("padding: 12px 30px; border-radius: 6px; "
                                    "border: 1.5px solid rgba(0, 212, 255, 0.7); border-top: 1.5px solid rgba(140, 215, 245, 0.9); border-left: 1.5px solid rgba(110, 200, 235, 0.85); border-right: 1.5px solid rgba(0, 80, 110, 0.9); border-bottom: 1.5px solid rgba(0, 55, 80, 0.95);"
                                    "background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 rgba(44, 58, 74, 0.9), stop:1 rgba(18, 24, 32, 0.95)); "
                                    "color: #00d4ff; font-size: 14px; font-weight: bold; "
                                    "box-shadow: inset 0 1px 2px rgba(0, 212, 255, 0.22), inset 0 -2px 3px rgba(0,0,0,0.45), 0 0 8px rgba(0, 212, 255, 0.12), 0 3px 8px rgba(0,0,0,0.6);");
}

/**
 * @brief 更换航线按钮点击事件处理
 * @details 实现更换航线逻辑：点击后按钮变绿色，表示航线更换中，弹窗关闭后恢复原始样式
 */
void Widget::onRouteReplaceClicked()
{
    // ==================== 检查前置条件 ====================
    if (!isPowerOn) {
        QMessageBox::warning(this, "警告", "请先开启电源");
        return;
    }
    
    // ==================== 检查目的地输入 ====================
    QString destination = leDestination->text().trimmed();
    if (destination.isEmpty()) {
        QMessageBox::warning(this, "提示", "请先输入目的地地址");
        return;
    }
    
    // 将更换航线按钮样式改为绿色状态色立体效果，表示航线更换中（绿色状态色）
    btnRouteReplace->setStyleSheet("width: 80px; height: 40px; border-radius: 20px; "
                                   "border: 1.5px solid #00e676; border-top: 1.5px solid #66ffaa; border-left: 1.5px solid #4dffa0; border-right: 1.5px solid #008040; border-bottom: 1.5px solid #006030;"
                                   "background: qradialgradient(cx:0.35, cy:0.3, radius:0.85, fx:0.35, fy:0.3, stop:0 rgba(0, 230, 118, 0.5), stop:0.6 rgba(0, 110, 55, 0.9), stop:1 rgba(0, 50, 25, 0.98)); "
                                   "color: #00e676; font-size: 13px; font-weight: bold; text-align: center; "
                                   "box-shadow: inset 0 1px 2px rgba(255,255,255,0.25), 0 0 16px rgba(0, 230, 118, 0.4), 0 3px 8px rgba(0,140,70,0.6);");
    
    // 使用高德地理编码API获取目的地坐标，限定在当前城市
    QString key = "7e7bd3381de08c627a637eba7c9a29ec";
    QString encodedDest = QString::fromUtf8(QUrl::toPercentEncoding(destination));
    QString cityForSearch = currentCity.isEmpty() ? "成都" : currentCity;
    QString encodedCity = QString::fromUtf8(QUrl::toPercentEncoding(cityForSearch));

    QString urlStr = QString("http://restapi.amap.com/v3/geocode/geo?address=%1&city=%2&output=json&key=%3")
                     .arg(encodedDest)
                     .arg(encodedCity)
                     .arg(key);
    
    QUrl qurl(urlStr);
    QNetworkRequest request(qurl);
    
    QNetworkReply *reply = networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, [=]() {
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray data = reply->readAll();
            QJsonDocument jsonDoc = QJsonDocument::fromJson(data);
            
            if (!jsonDoc.isNull() && jsonDoc.isObject()) {
                QJsonObject jsonObj = jsonDoc.object();
                if (jsonObj["status"].toString() == "1") {
                    QJsonArray geocodes = jsonObj["geocodes"].toArray();
                    if (!geocodes.isEmpty()) {
                        QJsonObject result = geocodes[0].toObject();
                        QString locationStr = result["location"].toString();
                        QStringList coords = locationStr.split(",");
                        if (coords.size() == 2) {
                            destLongitude = coords[0].toDouble();
                            destLatitude = coords[1].toDouble();
                            
                            if (destLatitude != 0 && destLongitude != 0) {
                                // 计算距离并更新显示
                                double distance = calculateDistance(latitude, longitude, destLatitude, destLongitude);
                                // 注意：totalDistance不重置，保持累计总行驶距离
                                traveledDistance = 0.0;
                                destinationDistance = distance;
                                lblTotalDistance->setText("总行驶距离：" + QString::number(totalDistance, 'f', 2) + "km");
                                lblTraveledDistance->setText("已行驶距离：" + QString::number(traveledDistance, 'f', 2) + "km");
                                lblDistance->setText("目的地距离：" + QString::number(destinationDistance, 'f', 2) + "km");
                                
                                qDebug() << "[航线] 当前位置:" << longitude << latitude << "目的地:" << destLongitude << destLatitude << "距离:" << distance << "km";
                                
                                // 弹窗显示路线地图
                                if (!mapDialog) {
                                    mapDialog = new MapDialog(this);
                                    // 地图弹窗关闭时恢复位置地图显示
                                    connect(mapDialog, &MapDialog::closed, this, [this]() {
                                        if (m_positionMapWasVisible && positionWebView && isPowerOn) {
                                            positionWebView->show();
                                            syncPositionMapGeometry();
                                        }
                                    });
                                }
                                // 隐藏位置地图，避免覆盖弹窗
                                if (positionWebView && positionWebView->isVisible()) {
                                    m_positionMapWasVisible = true;
                                    positionWebView->hide();
                                } else {
                                    m_positionMapWasVisible = false;
                                }
                                mapDialog->setRoute(longitude, latitude, destLongitude, destLatitude);
                                mapDialog->show();
                            } else {
                                QMessageBox::warning(this, "提示", "未能获取目的地坐标");
                            }
                        }
                    } else {
                        QMessageBox::warning(this, "提示", "未找到匹配的地址");
                    }
                } else {
                    QMessageBox::warning(this, "提示", "地理编码失败");
                }
            }
        } else {
            QMessageBox::warning(this, "提示", "网络请求失败，请检查网络连接");
        }
        reply->deleteLater();
        
        // 恢复按钮原始样式（磨砂深灰 + 青蓝描边）
        btnRouteReplace->setStyleSheet("width: 80px; height: 40px; border-radius: 20px; "
                                       "border: 1.5px solid rgba(0, 212, 255, 0.7); border-top: 1.5px solid rgba(140, 215, 245, 0.9); border-left: 1.5px solid rgba(110, 200, 235, 0.85); border-right: 1.5px solid rgba(0, 80, 110, 0.9); border-bottom: 1.5px solid rgba(0, 55, 80, 0.95);"
                                       "background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 rgba(44, 58, 74, 0.9), stop:1 rgba(18, 24, 32, 0.95)); "
                                       "color: #00d4ff; font-size: 13px; font-weight: bold; text-align: center; "
                                       "box-shadow: inset 0 1px 2px rgba(0, 212, 255, 0.22), inset 0 -2px 3px rgba(0,0,0,0.45), 0 0 8px rgba(0, 212, 255, 0.12), 0 3px 8px rgba(0,0,0,0.6);");
    });
}

/**
 * @brief 更新飞行数据函数
 * @details 模拟实时更新飞行数据，包括高度、速度、电量、重量和位置信息，并更新界面显示
 *          数据达到上下限时会自动循环重置，形成周期性模拟效果
 */
void Widget::updateFlightData()
{
    // ==================== 悬停状态：根据模式执行不同行为 ====================
    if (isHovering) {
        // 根据用户选择的悬停模式执行对应逻辑
        switch (hoverMode) {
        case CIRCLE_HOVER:
            if (isRotating && rotationTargetAngle > 0) {
                // 旋转模式：飞行器以1km/h速度旋转指定角度
                flightSpeed = 1.0;
                // 每次旋转10度（每秒10度，需要36秒完成360度）
                hoverHeading += 10.0;
                rotationCurrentAngle += 10.0;
                if (hoverHeading >= 360.0) hoverHeading -= 360.0;

                // 检查是否达到目标角度
                if (rotationCurrentAngle >= rotationTargetAngle) {
                    // 旋转完成，停止旋转
                    isRotating = false;
                    rotationCurrentAngle = 0;
                    rotationTargetAngle = 0;
                    // 切换到悬停模式（原地不动）
                    hoverMode = FORWARD_HOVER;
                }
            } else {
                // 普通盘旋模式：飞行器以1km/h的速度原地360°缓慢转圈
                flightSpeed = 1.0;  // 盘旋时以1km/h速度转圈
                hoverHeading += 10.0;  // 每秒旋转10度，模拟飞行器缓慢自转
                if (hoverHeading >= 360.0) hoverHeading = 0.0;  // 达到360°后重置，继续旋转
            }
            break;

        case FORWARD_HOVER:
            // 悬停模式：飞行器停留在原地不动
            flightSpeed = 0;  // 速度为0，不移动
            break;

        case BACKWARD_HOVER:
            // 后退悬停模式：飞行器以1km/h的速度缓慢后退，最多持续60秒
            flightSpeed = 1.0;  // 后退速度同样为1km/h
            latitude -= 0.00001;   // 纬度每次减少约1米（向南移动）
            longitude -= 0.00001;  // 经度每次减少约1米（向西移动）
            hoverBackwardSeconds++;  // 累加后退悬停已持续的秒数

            if (hoverBackwardSeconds >= 60) {
                // 超过60秒限制，自动切换到盘旋悬停模式并弹窗提示用户
                hoverMode = CIRCLE_HOVER;      // 强制切换到盘旋模式
                hoverBackwardSeconds = 0;      // 重置计时器
                QMessageBox::information(this, "提示",
                    "后退悬停时长超过一分钟，已自动切换到盘旋悬停模式");
            }
            break;
        }

        // 更新界面显示（悬停状态下的速度和坐标）
        lblFlightSpeed->setText("速度: " + QString::number(flightSpeed, 'f', 2) + "km/h");
        lblCoordinates->setText("纬度: " + QString::number(latitude, 'f', 6) + "°N\n"
                               + "经度: " + QString::number(longitude, 'f', 6) + "°E");

        // 悬停时持续消耗电池电量
        consumeBattery(1);
        updateShortcutButtonHighlight();  // 根据当前高度和速度数据更新快捷按钮高亮状态
        return;  // 悬停时直接返回，不执行后续的正常飞行/降落逻辑
    }

    // ==================== 降落状态：数据递减直到为0 ====================
    if (isLanding) {
        flightHeight -= 10.0;     // 降落时高度每次减少10米（模拟快速下降）
        flightSpeed -= 5.0;       // 降落时速度每次减少5 km/h（模拟减速）

        // 非负限制（第一重防护）：降落过程中高度和速度不能出现负数
        if (flightHeight < 0) flightHeight = 0;
        if (flightSpeed < 0) flightSpeed = 0;

        // 持续消耗电池电量
        consumeBattery(1);
        
        // 当高度和速度都降为0时，表示降落完成
        if (flightHeight <= 0 && flightSpeed <= 0) {
            isLanding = false;   // 降落完成，设置为非降落状态
            isFlying = false;    // 降落完成，设置为非飞行状态（已落地）
            dataUpdateTimer->stop();       // 停止数据更新定时器
            flightDurationTimer->stop();   // 停止飞行时长定时器
            mapUpdateTimer->stop();        // 停止地图位置更新定时器
            
            // 将降落按钮样式恢复为默认的浅青色立体效果
            QString defaultStyle = "width: 55px; height: 55px; border-radius: 27px; "
                                   "border: 1.5px solid rgba(0, 212, 255, 0.7); border-top: 1.5px solid rgba(140, 215, 245, 0.9); border-left: 1.5px solid rgba(110, 200, 235, 0.85); border-right: 1.5px solid rgba(0, 80, 110, 0.9); border-bottom: 1.5px solid rgba(0, 55, 80, 0.95);"
                                   "background: qradialgradient(cx:0.3, cy:0.25, radius:0.9, fx:0.3, fy:0.25, stop:0 rgba(82, 108, 138, 0.95), stop:0.4 rgba(42, 56, 72, 0.93), stop:0.8 rgba(20, 28, 38, 0.97), stop:1 rgba(6, 10, 16, 1)); "
                                   "color: #00d4ff; font-size: 12px; font-weight: bold; text-align: center; "
                                   "box-shadow: inset 0 1px 2px rgba(0, 212, 255, 0.25), inset 0 -2px 4px rgba(0,0,0,0.5), 0 0 10px rgba(0, 212, 255, 0.15), 0 3px 8px rgba(0,0,0,0.65);";
            btnLand->setStyleSheet(defaultStyle);
            btnLand->style()->unpolish(btnLand);
            btnLand->style()->polish(btnLand);
            
            // 降落完成后，重新启用高度和速度快捷按钮（电源仍开启时）
            if (isPowerOn) {
                setShortcutButtonsEnabled(true);
            }
        }
    } else {
        // ==================== 正常飞行状态：数据正常更新 ====================
        // 先起飞到目标高度，到达后固定在该高度飞行
        if (flightHeight < targetFlightHeight) {
            flightHeight += 5.0;  // 未达到目标高度，继续上升
            if (flightHeight > targetFlightHeight) flightHeight = targetFlightHeight;  // 限制不超过目标高度
        } else if (flightHeight > targetFlightHeight) {
            // 如果当前高度超过目标高度（可能因快捷按钮调整），下降回目标高度
            flightHeight -= 5.0;
            if (flightHeight < targetFlightHeight) flightHeight = targetFlightHeight;
        }
        if (flightHeight < 0) flightHeight = 0;  // 非负限制：高度不能为负

        // 速度控制逻辑：根据最大速度限制加速或减速
        if (flightSpeed > maxFlightSpeed) {
            // 当最大速度小于当前速度时（用户调低了最大速度），自动减速
            flightSpeed -= 2.0;   // 每次减少2 km/h
            if (flightSpeed < maxFlightSpeed) flightSpeed = maxFlightSpeed;  // 限制不低于最大速度
        } else if (flightHeight >= targetFlightHeight && flightSpeed < maxFlightSpeed) {
            // 只有高度达到目标高度后才开始加速（模拟真实飞行逻辑）
            flightSpeed += 2.0;   // 飞行速度每次增加2 km/h
            if (flightSpeed > maxFlightSpeed) flightSpeed = maxFlightSpeed;  // 限制不超过最大速度
        }
        if (flightSpeed < 0) flightSpeed = 0;  // 非负限制：速度不能为负

        // 正常飞行时持续消耗电池电量
        consumeBattery(1);
        latitude += 0.0001;      // 纬度每次增加0.0001度，模拟向北移动
        longitude += 0.0001;     // 经度每次增加0.0001度，模拟向东移动
        traveledDistance += 0.05;  // 已行驶距离缓慢增加
        // 注意：totalDistance为累计值，不与路线距离挂钩
        destinationDistance = qMax(0.0, destinationDistance - 0.05);  // 目的地剩余距离随行驶减少
        
        // 目的地距离到达0后，弹窗提示并自动进入悬停状态
        if (destinationDistance <= 0) {
            destinationDistance = 0;  // 限制最小值为0
            
            // 弹窗提示"已到达目的地"
            QMessageBox::information(this, "提示", "已到达目的地");
            
            // 自动进入悬停状态
            isHovering = true;
            isLanding = false;
            
            // 将悬停按钮样式改为红色立体效果，表示悬停状态
            QString hoverRedStyle = "width: 55px; height: 55px; border-radius: 27px; "
                                    "border: 1.5px solid #ff5252; border-top: 1.5px solid #ff9999; border-left: 1.5px solid #ff7777; border-right: 1.5px solid #aa2020; border-bottom: 1.5px solid #881515;"
                                    "background: qradialgradient(cx:0.35, cy:0.3, radius:0.85, fx:0.35, fy:0.3, stop:0 rgba(255, 100, 100, 0.55), stop:0.6 rgba(180, 30, 30, 0.9), stop:1 rgba(100, 15, 15, 0.98)); "
                                    "color: #ff5252; font-size: 12px; font-weight: bold; text-align: center; "
                                    "box-shadow: inset 0 2px 4px rgba(255,255,255,0.25), 0 0 25px rgba(255, 100, 100, 0.5), 0 3px 8px rgba(150,0,0,0.7);";
            btnHover->setStyleSheet(hoverRedStyle);
            btnHover->style()->unpolish(btnHover);
            btnHover->style()->polish(btnHover);
            
            // 恢复起飞和降落按钮的默认样式（状态互斥）
            QString defaultStyle = "width: 55px; height: 55px; border-radius: 27px; "
                                   "border: 1.5px solid rgba(0, 212, 255, 0.7); border-top: 1.5px solid rgba(140, 215, 245, 0.9); border-left: 1.5px solid rgba(110, 200, 235, 0.85); border-right: 1.5px solid rgba(0, 80, 110, 0.9); border-bottom: 1.5px solid rgba(0, 55, 80, 0.95);"
                                   "background: qradialgradient(cx:0.3, cy:0.25, radius:0.9, fx:0.3, fy:0.25, stop:0 rgba(82, 108, 138, 0.95), stop:0.4 rgba(42, 56, 72, 0.93), stop:0.8 rgba(20, 28, 38, 0.97), stop:1 rgba(6, 10, 16, 1)); "
                                   "color: #00d4ff; font-size: 12px; font-weight: bold; text-align: center; "
                                   "box-shadow: inset 0 1px 2px rgba(0, 212, 255, 0.25), inset 0 -2px 4px rgba(0,0,0,0.5), 0 0 10px rgba(0, 212, 255, 0.15), 0 3px 8px rgba(0,0,0,0.65);";
            btnTakeoff->setStyleSheet(defaultStyle);
            btnTakeoff->style()->unpolish(btnTakeoff);
            btnTakeoff->style()->polish(btnTakeoff);
            btnLand->setStyleSheet(defaultStyle);
            btnLand->style()->unpolish(btnLand);
            btnLand->style()->polish(btnLand);
            
            // 悬停时启用高度和速度快捷按钮
            setShortcutButtonsEnabled(true);

            updateShortcutButtonHighlight();  // 根据当前高度和速度数据更新快捷按钮高亮状态
            return;  // 提前返回，不继续执行后续代码
        }
        
        // ==================== 电池耗尽后的循环重置逻辑由 consumeBattery 函数处理 ====================
    }
    
    // ==================== 更新主界面数据显示 ====================
    // 非负限制（第二重防护）：显示前再次确保所有数据不为负数
    // 这是双重保护机制，防止任何边界情况下出现负数显示
    if (flightHeight < 0) flightHeight = 0;
    if (flightSpeed < 0) flightSpeed = 0;
    if (maxFlightSpeed < 0) maxFlightSpeed = 0;
    if (flightWeight < 0) flightWeight = 0;
    if (flightSeconds < 0) flightSeconds = 0;
    if (totalFlightSeconds < 0) totalFlightSeconds = 0;
    if (destinationDistance < 0) destinationDistance = 0;

    // 更新界面标签显示
    lblFlightHeight->setText("高度: "+QString::number(flightHeight, 'f', 2) + "m");  // 飞行高度保留2位小数
    lblFlightSpeed->setText("速度: "+QString::number(flightSpeed, 'f', 2) + "km/h");  // 飞行速度保留2位小数
    lblCoordinates->setText("纬度: "+QString::number(latitude, 'f', 6) + "°N\n" + "经度: "+QString::number(longitude, 'f', 6) + "°E");  // 坐标保留6位小数
    lblBatteryLevel->setText("电量");
    updateBatteryDisplay();  // 更新电池显示

    lblDistance->setText("目的地距离："+QString::number(destinationDistance, 'f', 2) + "km");
    lblTotalDistance->setText("总行驶距离：" + QString::number(totalDistance, 'f', 2) + "km");
    lblTraveledDistance->setText("已行驶距离：" + QString::number(traveledDistance, 'f', 2) + "km");

    // 重量保持不变（飞行过程中重量模拟恒定），无需更新

    updateShortcutButtonHighlight();  // 根据当前高度和速度数据更新快捷按钮高亮状态
}

/**
 * @brief 刷新界面函数
 * @details 将界面所有数据标签重置为初始值（0或空），用于开机或关机时清空界面显示
 */
void Widget::refreshUI()
{
    // ==================== 将界面所有数据设置为初始值（0） ====================
    lblFlightHeight->setText("高度: 0.00m");                    // 飞行高度重置为0.0米
    lblFlightSpeed->setText("速度: 0.00km/h");                  // 飞行速度重置为0.0 km/h
    lblCoordinates->setText("纬度: 0.000000°N\n经度: 0.000000°E");   // 坐标重置为0度，纬度和经度分行显示
    lblBatteryLevel->setText("电量");                          // 电池电量标签重置
    for (int i = 0; i < 6; i++) {
        batteryPercentages[i] = 0;
        batteryInstalled[i] = false;
    }
    currentBatteryIndex = 0;
    lowBatteryWarned = false;
    batteryLevel = 0;
    updateBatteryDisplay();
    
    lblWeight->setText("总载重: —— ——");                          // 总载重标签重置显示
    lblWeightValue->setText("限重120.00kg");                     // 限重标签重置显示
    lblWeightValue->setStyleSheet("");                         // 重置重量标签样式
    totalDistance = 0.0;
    traveledDistance = 0.0;
    destinationDistance = 0.0;
    lblTotalDistance->setText("总行驶距离：0.00km");                // 总行驶距离重置为0.00 km
    lblTraveledDistance->setText("已行驶距离：0.00km");              // 已行驶距离重置为0.00 km
    lblDistance->setText("目的地距离：0.00km");                // 目的地距离重置为0.00 km
    lblDurationValue->setText("00:00:00");                    // 总飞行时长重置为00:00:00
    lblCurrentDurationValue->setText("00:00:00");             // 当前飞行时长重置为00:00:00
    setPositionStatus("");                                    // 清除状态显示
    
    // ==================== 时间标签不在此处重置 ====================
    // 时间显示由 timeUpdateTimer 驱动 updateTime() 持续刷新，与电源状态无关
}

/**
 * @brief 消耗当前电池电量
 * @param amount 消耗的电量百分比
 * @details 从当前正在使用的电池中扣除电量。
 *          如果当前电池耗尽，自动切换到下一节电池。
 *          当使用到最后一节电池（第6节）且电量低于20%时，弹出低电量警告。
 *          所有电池耗尽后，自动重新充满6节电池。
 */
void Widget::consumeBattery(int amount)
{
    // 步骤1: 若当前电池未安装，需要跳转到下一节已安装的电池
    if (!batteryInstalled[currentBatteryIndex]) {
        // 从当前位置开始循环查找下一节已安装的电池（循环遍历6节电池）
        int next = currentBatteryIndex;
        for (int i = 1; i <= 6; i++) {
            int idx = (currentBatteryIndex + i) % 6;  // 取模实现循环遍历
            if (batteryInstalled[idx]) { next = idx; break; }
        }
        currentBatteryIndex = next;  // 切换到找到的已安装电池
    }

    // 步骤2: 从当前电池扣除指定电量
    if (batteryInstalled[currentBatteryIndex]) {
        batteryPercentages[currentBatteryIndex] -= amount;  // 扣除电量
        if (batteryPercentages[currentBatteryIndex] < 0) {
            batteryPercentages[currentBatteryIndex] = 0;  // 电量不能为负，限制最小值为0
        }
    }

    // 步骤3: 检查当前电池是否已耗尽（电量<=0）
    if (batteryPercentages[currentBatteryIndex] <= 0) {
        // 寻找下一节已安装且仍有电的电池
        int nextIdx = -1;
        for (int i = 1; i <= 6; i++) {
            int idx = (currentBatteryIndex + i) % 6;
            if (batteryInstalled[idx] && batteryPercentages[idx] > 0) {
                nextIdx = idx;  // 找到可用电池
                break;
            }
        }

        if (nextIdx >= 0) {
            // 成功找到下一节可用电池，切换过去
            currentBatteryIndex = nextIdx;
            lowBatteryWarned = false;  // 切换到新电池后重置低电量警告标志
        } else {
            // 所有已安装电池都耗尽了：自动重新充满所有已安装电池
            for (int i = 0; i < 6; i++) {
                if (batteryInstalled[i]) {
                    batteryPercentages[i] = 100;  // 已安装电池重新充满至100%
                }
                // 未安装的电池保持不变
            }
            // 找到第一节已安装的电池作为当前使用电池
            for (int i = 0; i < 6; i++) {
                if (batteryInstalled[i]) {
                    currentBatteryIndex = i;
                    break;
                }
            }
            lowBatteryWarned = false;
        }
    }

    // 步骤4: 检查是否只剩最后一节已安装电池且电量低
    // 统计已安装且仍有电的电池数量
    int installedWithPower = 0;
    int lastInstalledIdx = -1;
    for (int i = 0; i < 6; i++) {
        if (batteryInstalled[i] && batteryPercentages[i] > 0) {
            installedWithPower++;  // 统计有电的电池数量
            lastInstalledIdx = i;   // 记录最后一节有电电池的索引
        }
    }

    // 只剩最后一节已安装电池且电量<=20%时弹窗警告（仅弹窗一次）
    if (installedWithPower == 1 && lastInstalledIdx == currentBatteryIndex
        && !lowBatteryWarned
        && batteryPercentages[currentBatteryIndex] <= 20
        && batteryPercentages[currentBatteryIndex] > 0) {
        lowBatteryWarned = true;  // 标记已弹窗，避免重复警告
        QMessageBox::warning(this, "电池电量不足",
            "电池电量不足，请尽快更换电池！\n\n"
            "当前使用：第 " + QString::number(currentBatteryIndex + 1) + " / 6 节\n"
            "剩余电量：" + QString::number(batteryPercentages[currentBatteryIndex]) + "%");
    }

    // 步骤5: 更新总电量显示
    batteryLevel = getTotalBatteryLevel();  // 重新计算总电量百分比
    updateBatteryDisplay();                  // 刷新电池UI显示
}

/**
 * @brief 获取总电量百分比
 * @return 所有6节电池位置的平均电量百分比
 * @details 总电量 = 所有电池电量之和 / 6
 *          未安装的电池电量按0计算，因此有未安装电池时总电量不会是100%
 */
int Widget::getTotalBatteryLevel() const
{
    int total = 0;
    for (int i = 0; i < 6; i++) {
        if (batteryInstalled[i]) {
            total += batteryPercentages[i];  // 已安装电池累加实际电量
        }
        // 未安装的电池按0计算，因此存在未安装电池时总电量不会达到100%
    }
    return total / 6;  // 总电量 = 所有电池电量之和 / 6，取平均值
}

/**
 * @brief 更新6节电池的显示
 * @details 根据每节电池的独立电量和当前使用的电池编号，
 *          更新电池能量条颜色、总电量百分比、当前电池编号显示
 *          未安装和已耗尽的电池都显示为灰色
 */
void Widget::updateBatteryDisplay()
{
    // 步骤1: 更新总电量百分比标签和进度条
    lblBatteryPercent->setText(QString::number(batteryLevel) + "%");
    batteryBar->setValue(batteryLevel);

    // 步骤2: 遍历6节电池，根据状态设置显示颜色
    for (int i = 0; i < 6; i++) {
        int pct = batteryPercentages[i];                    // 该节电池的电量百分比
        bool isActive = (i == currentBatteryIndex);         // 是否为当前正在使用的电池
        bool isInstalled = batteryInstalled[i];             // 是否已安装
        bool isEmpty = (pct <= 0);                          // 是否已耗尽

        QString colorStyle;
        if (!isInstalled || isEmpty) {
            // 未安装 或 已安装但没电：显示暗灰色（统一视觉效果）
            colorStyle = "background: rgba(50, 55, 62, 0.5); border-radius: 4px; border: 1px solid rgba(80, 90, 100, 0.5);";
        } else if (isActive) {
            // 当前使用的电池：青蓝高亮，便于识别（主色）
            colorStyle = "background: #00d4ff; border-radius: 4px; border: 1px solid rgba(0, 212, 255, 0.9);";
        } else if (pct > 60) {
            // 高电量（>60%）：绿色，表示电量充足（状态色）
            colorStyle = "background: #00e676; border-radius: 4px; border: 1px solid rgba(0, 230, 118, 0.8);";
        } else if (pct > 30) {
            // 中电量（30%-60%）：黄色，提示注意（警示色）
            colorStyle = "background: #ffd600; border-radius: 4px; border: 1px solid rgba(255, 214, 0, 0.8);";
        } else {
            // 低电量（<30%）：红色，警告电量不足
            colorStyle = "background: #ff5252; border-radius: 4px; border: 1px solid rgba(255, 82, 82, 0.8);";
        }

        batteryBars[i]->setStyleSheet(colorStyle);  // 应用样式到对应的电池条
    }

    // 步骤3: 更新当前电池编号标签，显示格式为"电池 X/Y"
    if (lblBatteryIndex) {
        // 统计已安装电池总数
        int installedCount = 0;
        for (int i = 0; i < 6; i++) {
            if (batteryInstalled[i]) installedCount++;
        }
        QString label = QString("电池 %1/%2").arg(currentBatteryIndex + 1).arg(installedCount);
        if (installedCount == 1) {
            // 只剩1节电池时显示警告标识，红色样式
            label += " ⚠";
            lblBatteryIndex->setStyleSheet(
                "color: #ff5252; font-size: 10px; padding: 2px 6px; "
                "border: 1px solid #ff5252; border-radius: 3px; "
                "background: rgba(255,82,82,0.12);");
        } else {
            // 正常情况下显示青蓝样式
            lblBatteryIndex->setStyleSheet(
                "color: #00d4ff; font-size: 10px; padding: 2px 6px; "
                "border: 1px solid rgba(0, 212, 255, 0.5); border-radius: 3px; "
                "background: rgba(0, 212, 255, 0.08);");
        }
        lblBatteryIndex->setText(label);
    }

    // 步骤4: 计算并更新当前电量可飞行距离
    // 模型：每1%电量支持1秒飞行（consumeBattery每秒扣1%），剩余飞行时间 = 已安装电池剩余电量总和（秒）
    // 可飞行距离 = 剩余时间（小时）× 巡航速度（km/h），巡航速度取当前设定的最大速度
    if (lblFlyableDistance) {
        int totalRemainingPct = 0;  // 已安装电池剩余电量总和（%）
        for (int i = 0; i < 6; i++) {
            if (batteryInstalled[i]) totalRemainingPct += batteryPercentages[i];
        }
        double remainingHours = totalRemainingPct / 3600.0;  // 秒→小时
        double cruiseSpeed = maxFlightSpeed;                  // 巡航速度取设定的最大速度
        double flyableDistance = remainingHours * cruiseSpeed;
        lblFlyableDistance->setText("<span>可飞行距离：</span><span style='color:#00e676;'>" + QString::number(flyableDistance, 'f', 2) + "km</span>");
    }
}

/**
 * @brief 重置按钮样式函数
 * @details 将所有操作按钮恢复到默认立体样式（深色背景、渐变效果、阴影），
 *          同时将灯光控件恢复到关闭状态，用于关机时重置界面状态
 */
void Widget::resetButtonStyles()
{
    // ==================== 应用默认样式到圆形按钮（磨砂深灰 + 青蓝描边） ====================
    QString defaultStyle = "width: 55px; height: 55px; border-radius: 27px; "
                           "border: 1.5px solid rgba(0, 212, 255, 0.7); border-top: 1.5px solid rgba(140, 215, 245, 0.9); border-left: 1.5px solid rgba(110, 200, 235, 0.85); border-right: 1.5px solid rgba(0, 80, 110, 0.9); border-bottom: 1.5px solid rgba(0, 55, 80, 0.95);"
                           "background: qradialgradient(cx:0.3, cy:0.25, radius:0.9, fx:0.3, fy:0.25, stop:0 rgba(82, 108, 138, 0.95), stop:0.4 rgba(42, 56, 72, 0.93), stop:0.8 rgba(20, 28, 38, 0.97), stop:1 rgba(6, 10, 16, 1)); "
                           "color: #00d4ff; font-size: 12px; font-weight: bold; text-align: center; "
                           "box-shadow: inset 0 1px 2px rgba(0, 212, 255, 0.25), inset 0 -2px 4px rgba(0,0,0,0.5), 0 0 10px rgba(0, 212, 255, 0.15), 0 3px 8px rgba(0,0,0,0.65);";

    btnTakeoff->setStyleSheet(defaultStyle);
    btnTakeoff->style()->unpolish(btnTakeoff);
    btnTakeoff->style()->polish(btnTakeoff);
    btnHover->setStyleSheet(defaultStyle);
    btnHover->style()->unpolish(btnHover);
    btnHover->style()->polish(btnHover);
    btnLand->setStyleSheet(defaultStyle);
    btnLand->style()->unpolish(btnLand);
    btnLand->style()->polish(btnLand);

    // ==================== 指挥中心按钮恢复默认样式（字体稍小，11号） ====================
    btnCommandCenter->setStyleSheet("QPushButton { width: 55px; height: 55px; border-radius: 27px; "
                                    "border: 1.5px solid rgba(0, 212, 255, 0.7); border-top: 1.5px solid rgba(140, 215, 245, 0.9); border-left: 1.5px solid rgba(110, 200, 235, 0.85); border-right: 1.5px solid rgba(0, 80, 110, 0.9); border-bottom: 1.5px solid rgba(0, 55, 80, 0.95);"
                                    "background: qradialgradient(cx:0.3, cy:0.25, radius:0.9, fx:0.3, fy:0.25, stop:0 rgba(82, 108, 138, 0.95), stop:0.4 rgba(42, 56, 72, 0.93), stop:0.8 rgba(20, 28, 38, 0.97), stop:1 rgba(6, 10, 16, 1)); "
                                    "color: #00d4ff; font-size: 11px; font-weight: bold; text-align: center; "
                                    "box-shadow: inset 0 1px 2px rgba(0, 212, 255, 0.25), inset 0 -2px 4px rgba(0,0,0,0.5), 0 0 10px rgba(0, 212, 255, 0.15), 0 3px 8px rgba(0,0,0,0.65); }");

    // ==================== 能源更换按钮恢复默认样式（矩形按钮，圆角6px） ====================
    btnEnergyReplace->setStyleSheet("padding: 12px 30px; border-radius: 6px; "
                                    "border: 1.5px solid rgba(0, 212, 255, 0.7); border-top: 1.5px solid rgba(140, 215, 245, 0.9); border-left: 1.5px solid rgba(110, 200, 235, 0.85); border-right: 1.5px solid rgba(0, 80, 110, 0.9); border-bottom: 1.5px solid rgba(0, 55, 80, 0.95);"
                                    "background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 rgba(44, 58, 74, 0.9), stop:1 rgba(18, 24, 32, 0.95)); "
                                    "color: #00d4ff; font-size: 14px; font-weight: bold; "
                                    "box-shadow: inset 0 1px 2px rgba(0, 212, 255, 0.22), inset 0 -2px 3px rgba(0,0,0,0.45), 0 0 8px rgba(0, 212, 255, 0.12), 0 3px 8px rgba(0,0,0,0.6);");

    // ==================== 更换航线按钮恢复默认样式（椭圆形按钮，圆角20px） ====================
    btnRouteReplace->setStyleSheet("width: 80px; height: 40px; border-radius: 20px; "
                                   "border: 1.5px solid rgba(0, 212, 255, 0.7); border-top: 1.5px solid rgba(140, 215, 245, 0.9); border-left: 1.5px solid rgba(110, 200, 235, 0.85); border-right: 1.5px solid rgba(0, 80, 110, 0.9); border-bottom: 1.5px solid rgba(0, 55, 80, 0.95);"
                                   "background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 rgba(44, 58, 74, 0.9), stop:1 rgba(18, 24, 32, 0.95)); "
                                   "color: #00d4ff; font-size: 13px; font-weight: bold; text-align: center; "
                                   "box-shadow: inset 0 1px 2px rgba(0, 212, 255, 0.22), inset 0 -2px 3px rgba(0,0,0,0.45), 0 0 8px rgba(0, 212, 255, 0.12), 0 3px 8px rgba(0,0,0,0.6);");

    // ==================== 灯光控件恢复默认状态（电源关闭时灯光也应关闭） ====================
    lightsState = 0;  // 灯光状态重置为关闭
    btnLights->setText("灯光");
    btnLights->setStyleSheet(lightsOffStyle);
    btnLowLight->setStyleSheet(lowLightOffStyle);
    btnHighLight->setStyleSheet(highLightOffStyle);
    btnLowLight->setEnabled(false);
    btnHighLight->setEnabled(false);
}

/**
 * @brief 初始化位置信息框动态地图
 * @details 加载 amap.html 页面（基于高德地图 JS API），实现卫星地图 + 路网图层的实时显示
 *          地图就绪后通过 checkPositionMapReady 检查并应用缓存坐标
 */
void Widget::initPositionMap()
{
    if (!positionWebView) return;
    qDebug() << "[位置地图] 开始加载 qrc:/amap.html ...";
    positionMapReady = false;
    positionRetryCount = 0;
    lastPosSentLon = 0;
    lastPosSentLat = 0;
    positionStatus = "地图加载中...";
    positionWebView->load(QUrl("qrc:/amap.html"));
}

/**
 * @brief 检查位置信息框动态地图是否就绪并应用坐标
 * @details 递归重试机制：通过 JS 调用 isMapReady 判断地图是否就绪
 *          就绪后标记 positionMapReady，并应用当前经纬度到地图
 *          最多重试 30 次（约 9 秒），超时后强制标记就绪避免死锁
 */
void Widget::checkPositionMapReady()
{
    if (!positionWebPage) return;

    positionWebPage->runJavaScript("isMapReady();", [this](const QVariant &result) {
        bool ready = result.toBool();
        if (ready) {
            qDebug() << "[位置地图] 地图已就绪";
            positionMapReady = true;
            positionRetryCount = 0;
            // 地图就绪后：同步窗口几何位置 + 显示窗口 + 强制刷新
            syncPositionMapGeometry();
            if (isPowerOn && positionWebView && positionWebView->isHidden()) {
                positionWebView->show();
            }
            QTimer::singleShot(100, this, [this]() {
                syncPositionMapGeometry();
                if (positionWebPage) positionWebPage->runJavaScript("_forceResize();");
            });
            // 重新应用缓存的状态文字（地图就绪前设置的状态此时才真正显示）
            if (!positionStatus.isEmpty()) {
                QString safe = positionStatus;
                safe.replace("'", "\\'");
                positionWebPage->runJavaScript(QString("setStatus('%1');").arg(safe));
            }
            // 应用当前坐标（若有效），坐标有效时会覆盖状态文字显示经纬度
            if (isCoordValid(longitude, latitude)) {
                lastPosSentLon = 0;
                lastPosSentLat = 0;
                updatePositionWebView(longitude, latitude);
            }
        } else if (positionRetryCount < 30) {
            positionRetryCount++;
            QTimer::singleShot(300, this, [this]() { checkPositionMapReady(); });
        } else {
            qWarning() << "[位置地图] 地图就绪检查超时，强制标记就绪";
            positionMapReady = true;
            positionRetryCount = 0;
            // 超时分支同样同步窗口几何位置 + 显示窗口
            syncPositionMapGeometry();
            if (isPowerOn && positionWebView && positionWebView->isHidden()) {
                positionWebView->show();
            }
            if (!positionStatus.isEmpty()) {
                QString safe = positionStatus;
                safe.replace("'", "\\'");
                positionWebPage->runJavaScript(QString("setStatus('%1');").arg(safe));
            }
            if (isCoordValid(longitude, latitude)) {
                lastPosSentLon = 0;
                lastPosSentLat = 0;
                updatePositionWebView(longitude, latitude);
            }
        }
    });
}

/**
 * @brief 更新位置信息框动态地图位置
 * @details 通过 runJavaScript 调用 JS 端 setPosition 实现实时位置更新
 *          地图未就绪时跳过；位置变化极小时跳过以减少 JS 调用开销
 * @param lon 经度
 * @param lat 纬度
 */
void Widget::updatePositionWebView(double lon, double lat, bool forceCenter)
{
    if (!positionWebPage || !positionMapReady) return;
    if (!isCoordValid(lon, lat)) return;

    // 位置去重：变化极小时跳过 JS 调用（约1米精度）
    // forceCenter=true 时跳过去重（确保真实位置一定被发送）
    if (!forceCenter) {
        const double minDiff = 0.00001;
        if (qAbs(lon - lastPosSentLon) < minDiff && qAbs(lat - lastPosSentLat) < minDiff) {
            return;
        }
    }
    lastPosSentLon = lon;
    lastPosSentLat = lat;

    // 将 forceCenter 参数传递给 JS 端的 setPosition
    QString js = QString("setPosition(%1, %2, %3);")
                 .arg(lon, 0, 'f', 6)
                 .arg(lat, 0, 'f', 6)
                 .arg(forceCenter ? "true" : "false");
    positionWebPage->runJavaScript(js, [js](const QVariant &result) {
        QString status = result.toString();
        if (status.startsWith("error:", Qt::CaseInsensitive)) {
            qWarning() << "[位置地图] setPosition 报错:" << status;
        }
    });
}

/**
 * @brief 设置位置信息框状态文字
 * @details 通过 JS 端 setStatus 更新地图左上角状态提示（如"定位中..."）
 *          地图未就绪时缓存状态文字，待就绪后由 checkPositionMapReady 流程处理
 * @param msg 状态文字（支持HTML），空字符串恢复默认提示
 */
void Widget::setPositionStatus(const QString &msg)
{
    positionStatus = msg;
    if (!positionWebPage || !positionMapReady) return;

    // 转义单引号，避免破坏 JS 字符串
    QString safe = msg;
    safe.replace("'", "\\'");
    positionWebPage->runJavaScript(QString("setStatus('%1');").arg(safe));
}

/**
 * @brief 同步位置信息框动态地图子窗口的几何位置
 * @details positionWebView 是 mapFrame 的子窗口，本方法将其精确定位到占位控件
 *          positionMapPlaceholder 在 mapFrame 内的相对位置。
 *          使用 mapToParent 获取占位控件相对于 mapFrame 的坐标，确保地图始终在框内。
 */
void Widget::syncPositionMapGeometry()
{
    if (!positionWebView || !positionMapPlaceholder) return;
    if (!isVisible()) return;

    // 获取占位控件相对于地图框的坐标（父窗口相对位置）
    QPoint relativePos = positionMapPlaceholder->mapTo(positionFramePtr, QPoint(0, 0));
    QSize targetSize = positionMapPlaceholder->size();

    if (targetSize.width() <= 0 || targetSize.height() <= 0) return;

    // 构建目标矩形（相对于 mapFrame 的父窗口坐标）
    QRect targetRect(relativePos, targetSize);

    if (positionWebView->geometry() != targetRect) {
        positionWebView->setGeometry(targetRect);
    }
    // 置顶确保不被其他子控件遮挡
    positionWebView->raise();
}

/**
 * @brief 加载地图到当前位置信息框（动态API地图）
 * @details 通过高德地图 JS API 实时显示卫星地图并定位到指定坐标
 *          地图未就绪时缓存坐标，待就绪后自动应用
 * @param lon 经度
 * @param lat 纬度
 */
void Widget::loadMapToPositionFrame(double lon, double lat)
{
    if (!isCoordValid(lon, lat)) {
        qDebug() << "[位置地图] 坐标无效，忽略:" << lon << "," << lat;
        return;
    }

    // 更新当前坐标
    longitude = lon;
    latitude = lat;

    if (positionMapReady && positionWebPage) {
        // 重置去重标记，确保新位置被发送
        lastPosSentLon = 0;
        lastPosSentLat = 0;
        // forceCenter=true：GPS/IP 定位成功获取真实位置，强制居中到真实位置
        updatePositionWebView(lon, lat, true);
    } else if (!positionMapReady) {
        // 地图未就绪，启动就绪检查（checkPositionMapReady 就绪后会自动应用坐标）
        if (positionRetryCount == 0) {
            checkPositionMapReady();
        }
    }
}

/**
 * @brief 鼠标滚轮事件处理
 * @details 位置信息框动态地图的滚轮缩放由 QWebEngineView（高德地图 JS API）原生处理，
 *          此处不再自定义缩放逻辑，直接交由父类处理
 */
void Widget::wheelEvent(QWheelEvent *event)
{
    QWidget::wheelEvent(event);
}

/**
 * @brief 地图按钮点击事件处理
 * @details 弹出地图窗口，显示当前位置和周围地图
 */
void Widget::onMapClicked()
{
    if (!isPowerOn) {
        QMessageBox::warning(this, "警告", "请先开启电源");
        return;
    }
    
    qDebug() << "[地图点击] 当前坐标: longitude=" << longitude << ", latitude=" << latitude;
    
    if (!mapDialog) {
        mapDialog = new MapDialog(this);
        // 地图弹窗关闭时恢复位置地图显示
        connect(mapDialog, &MapDialog::closed, this, [this]() {
            if (m_positionMapWasVisible && positionWebView && isPowerOn) {
                positionWebView->show();
                syncPositionMapGeometry();
            }
        });
    }
    // 隐藏位置地图，避免覆盖弹窗
    if (positionWebView && positionWebView->isVisible()) {
        m_positionMapWasVisible = true;
        positionWebView->hide();
    } else {
        m_positionMapWasVisible = false;
    }
    // 先设置坐标，再显示窗口，确保showEvent使用最新坐标
    mapDialog->setPosition(longitude, latitude);
    mapDialog->show();
    
    // 如果定位还在进行中（状态文字包含"正在获取"），重新触发定位
    if (positionStatus.contains("正在获取")) {
        requestRealLocation();
    }
}

/**
 * @brief 设置高度和速度快捷按钮的可用性
 * @details 根据电源状态和飞行状态控制按钮是否可点击
 * @param enabled 是否启用按钮
 */
void Widget::setShortcutButtonsEnabled(bool enabled)
{
    // 禁用按钮前清除所有内联米色样式，恢复默认样式，确保禁用后显示统一的disabled状态
    if (!enabled) {
        applyDefaultStyleToGroup(heightBtnGroup);                        // 高度按钮恢复默认内联样式
        applyDefaultStyleToGroup(speedBtnGroup);                        // 速度按钮恢复默认内联样式
    }
    btn1m5->setEnabled(enabled);
    btn2m->setEnabled(enabled);
    btn3m->setEnabled(enabled);
    btn0m6->setEnabled(enabled);
    btn1m0->setEnabled(enabled);
    btn3km->setEnabled(enabled);        // 设置3 km/h速度快捷按钮的启用状态
    btn5kmLow->setEnabled(enabled);     // 设置5 km/h速度快捷按钮的启用状态
    btn10m->setEnabled(enabled);
    btn25km->setEnabled(enabled);
    btn1km->setEnabled(enabled);
    btn5km->setEnabled(enabled);
}

/**
 * @brief 更新快捷按钮高亮状态
 * @details 根据当前目标飞行高度和最大速度数据，自动高亮（米色）对应的快捷按钮
 *          在起飞后、数据重置后调用，确保初始数据对应的按钮呈现被点击状态
 *          高度按钮之间互斥，速度按钮之间互斥，同一时间只有各一个按钮高亮
 *          使用浮点容差0.01进行比较，避免浮点精度问题
 */
void Widget::updateShortcutButtonHighlight()
{
    // ==================== 根据目标飞行高度匹配并高亮对应的高度快捷按钮 ====================
    // 依次检查每个高度按钮对应的值，与当前目标飞行高度进行比较（容差0.01m）
    if (qAbs(targetFlightHeight - 0.6) < 0.01) {
        setShortcutButtonBeige(btn0m6, heightBtnGroup);                 // 目标高度为0.6m，高亮0.60m按钮
    } else if (qAbs(targetFlightHeight - 1.0) < 0.01) {
        setShortcutButtonBeige(btn1m0, heightBtnGroup);                 // 目标高度为1.0m，高亮1.00m按钮
    } else if (qAbs(targetFlightHeight - 1.5) < 0.01) {
        setShortcutButtonBeige(btn1m5, heightBtnGroup);                 // 目标高度为1.5m，高亮1.50m按钮
    } else if (qAbs(targetFlightHeight - 2.0) < 0.01) {
        setShortcutButtonBeige(btn2m, heightBtnGroup);                  // 目标高度为2.0m，高亮2.00m按钮
    } else if (qAbs(targetFlightHeight - 3.0) < 0.01) {
        setShortcutButtonBeige(btn3m, heightBtnGroup);                  // 目标高度为3.0m，高亮3.00m按钮
    }

    // ==================== 根据最大速度匹配并高亮对应的速度快捷按钮 ====================
    // 依次检查每个速度按钮对应的值，与当前最大速度进行比较（容差0.01km/h）
    if (qAbs(maxFlightSpeed - 3.0) < 0.01) {
        setShortcutButtonBeige(btn3km, speedBtnGroup);                 // 当前最大速度为3km/h，高亮3.00km按钮
    } else if (qAbs(maxFlightSpeed - 5.0) < 0.01) {
        setShortcutButtonBeige(btn5kmLow, speedBtnGroup);              // 当前最大速度为5km/h，高亮5.00km按钮
    } else if (qAbs(maxFlightSpeed - 10.0) < 0.01) {
        setShortcutButtonBeige(btn10m, speedBtnGroup);                 // 当前最大速度为10km/h，高亮10.00km按钮
    } else if (qAbs(maxFlightSpeed - 15.0) < 0.01) {
        setShortcutButtonBeige(btn25km, speedBtnGroup);                // 当前最大速度为15km/h，高亮15.00km按钮
    } else if (qAbs(maxFlightSpeed - 25.0) < 0.01) {
        setShortcutButtonBeige(btn1km, speedBtnGroup);                 // 当前最大速度为25km/h，高亮25.00km按钮
    } else if (qAbs(maxFlightSpeed - 30.0) < 0.01) {
        setShortcutButtonBeige(btn5km, speedBtnGroup);                // 当前最大速度为30km/h，高亮30.00km按钮
    }
}

/**
 * @brief 将指定快捷按钮设置为米色高亮状态
 * @details 参考起飞/悬停/降落按钮的实现方式，通过 setStyleSheet 内联样式设置米色背景
 *          关键改进：始终使用内联样式（米色或默认），避免QSS与内联切换导致尺寸不一致
 *          调用前会先将同组其他按钮恢复默认内联样式，确保互斥效果
 * @param btn 需要设置为米色的按钮指针
 * @param group 该按钮所属的互斥分组（高度组heightBtnGroup或速度组speedBtnGroup）
 */
void Widget::setShortcutButtonBeige(QPushButton* btn, QButtonGroup* group)
{
    // ==================== 将同组所有按钮恢复默认内联样式 ====================
    // 使用默认内联样式而非空字符串，确保始终走内联渲染路径，避免尺寸不一致
    applyDefaultStyleToGroup(group);                                      // 同组所有按钮恢复默认

    // ==================== 为被点击的按钮设置米色内联样式 ====================
    // 米色样式的尺寸属性（padding/border-width/font-size）与默认样式完全一致
    // 仅改变颜色/背景/边框颜色，确保切换时按钮尺寸不变
    btn->setStyleSheet(buildShortcutBtnBeigeStyle());                     // 应用米色内联样式
    btn->style()->unpolish(btn);                                          // 取消旧样式
    btn->style()->polish(btn);                                            // 应用新样式（刷新显示）
}

/**
 * @brief 清除所有快捷按钮的高亮状态
 * @details 将所有高度和速度快捷按钮的内联样式恢复为默认样式
 *          同时取消选中状态（setChecked(false)），确保按钮完全恢复默认外观
 *          主要用于结束飞行或禁用按钮时重置快捷按钮的视觉状态
 */
void Widget::clearShortcutButtonHighlight()
{
    // ==================== 清除高度快捷按钮选中状态并恢复默认内联样式 ====================
    heightBtnGroup->setExclusive(false);                                  // 临时关闭互斥以允许取消选中
    btn0m6->setChecked(false);                                            // 取消0.6米按钮选中
    btn1m0->setChecked(false);                                            // 取消1.0米按钮选中
    btn1m5->setChecked(false);                                            // 取消1.5米按钮选中
    btn2m->setChecked(false);                                             // 取消2.0米按钮选中
    btn3m->setChecked(false);                                             // 取消3.0米按钮选中
    heightBtnGroup->setExclusive(true);                                   // 恢复互斥模式
    applyDefaultStyleToGroup(heightBtnGroup);                             // 高度按钮恢复默认内联样式

    // ==================== 清除速度快捷按钮选中状态并恢复默认内联样式 ====================
    speedBtnGroup->setExclusive(false);                                    // 临时关闭互斥以允许取消选中
    btn3km->setChecked(false);                                            // 取消3 km/h按钮选中
    btn5kmLow->setChecked(false);                                         // 取消5 km/h按钮选中
    btn10m->setChecked(false);                                            // 取消10 km/h按钮选中
    btn25km->setChecked(false);                                           // 取消15 km/h按钮选中
    btn1km->setChecked(false);                                            // 取消25 km/h按钮选中
    btn5km->setChecked(false);                                            // 取消30 km/h按钮选中
    speedBtnGroup->setExclusive(true);                                    // 恢复互斥模式
    applyDefaultStyleToGroup(speedBtnGroup);                              // 速度按钮恢复默认内联样式
}

/**
 * @brief 更新飞行时长函数
 * @details 每秒更新当前飞行时长，格式为HH:MM:SS
 *          总飞行时长在关机时累计保存，飞行过程中保持不变
 */
void Widget::updateFlightDuration()
{
    flightSeconds++;  // 当前飞行时长递增
    int hours = flightSeconds / 3600;
    int minutes = (flightSeconds % 3600) / 60;
    int seconds = flightSeconds % 60;
    // 更新当前飞行时长显示
    lblCurrentDurationValue->setText(QString("%1:%2:%3")
                                     .arg(hours, 2, 10, QChar('0'))
                                     .arg(minutes, 2, 10, QChar('0'))
                                     .arg(seconds, 2, 10, QChar('0')));
}

/**
 * @brief 更新地图位置函数
 * @details 将当前经纬度数据实时更新到地图弹窗
 */
void Widget::updateMapPosition()
{
    if (mapDialog && mapDialog->isVisible()) {
        mapDialog->updatePosition(longitude, latitude);
    }
    // 同步更新主界面位置信息框的动态地图
    updatePositionWebView(longitude, latitude);
}

/**
 * @brief 获取精确位置（跨平台实现）
 * @details Windows平台：使用QProcess调用PowerShell，通过WinRT Geolocation API获取精确经纬度
 *          Linux平台：直接使用高德IP定位API获取位置（回退方案）
 *          定位失败时回退到高德IP定位
 */
void Widget::requestRealLocation()
{
#ifdef Q_OS_WIN
    // Windows平台：使用PowerShell调用WinRT Geolocation API
    static const char* psScript =
        "$winrtDllPath = (Get-ChildItem 'C:\\Windows\\Microsoft.NET\\Assembly\\GAC_MSIL\\System.Runtime.WindowsRuntime' -Recurse -Filter 'System.Runtime.WindowsRuntime.dll' -ErrorAction SilentlyContinue | Select-Object -First 1 -ExpandProperty FullName); "
        "if ($winrtDllPath) { [System.Reflection.Assembly]::LoadFrom($winrtDllPath) | Out-Null }; "
        "[Windows.Devices.Geolocation.Geolocator, Windows.Devices.Geolocation, ContentType = WindowsRuntime] | Out-Null; "
        "[Windows.Devices.Geolocation.Geoposition, Windows.Devices.Geolocation, ContentType = WindowsRuntime] | Out-Null; "
        "$asTaskGeneric = ([System.WindowsRuntimeSystemExtensions].GetMethods() | Where-Object { $_.Name -eq 'AsTask' -and $_.GetParameters().Count -eq 1 -and $_.GetParameters()[0].ParameterType.Name -eq 'IAsyncOperation`1' })[0]; "
        "try { "
        "$locator = New-Object Windows.Devices.Geolocation.Geolocator; "
        "$methods = $locator.GetType().GetMethods() | Where-Object { $_.Name -eq 'GetGeopositionAsync' }; "
        "$method = $null; "
        "foreach ($m in $methods) { if ($m.GetParameters().Count -eq 0) { $method = $m; break } }; "
        "if ($method -eq $null) { Write-Output 'ERROR:NoMethod'; return }; "
        "$operation = $method.Invoke($locator, $null); "
        "if ($operation -eq $null) { Write-Output 'ERROR:NullOperation'; return }; "
        "$geopositionType = [Windows.Devices.Geolocation.Geoposition]; "
        "$asTaskMethod = $asTaskGeneric.MakeGenericMethod($geopositionType); "
        "$task = $asTaskMethod.Invoke($null, @($operation)); "
        "if ($task -ne $null -and $task.Wait(15000)) { "
        "$position = $task.Result; "
        "$coord = $position.Coordinate.Point.Position; "
        "Write-Output ('RESULT:' + $coord.Longitude + ',' + $coord.Latitude) "
        "} else { Write-Output 'ERROR:TIMEOUT' } "
        "} catch { Write-Output ('ERROR:' + $_.Exception.Message) }";

    if (!gpsProcess) {
        gpsProcess = new QProcess(this);
        connect(gpsProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, &Widget::onGpsLocationReceived);
        connect(gpsProcess, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
            requestIPLocation();
        });
    }

    // 通过标准输入传递脚本内容，避免文件路径问题
    QStringList args;
    args << "-ExecutionPolicy" << "Bypass" << "-NoProfile" << "-NonInteractive" << "-Command" << "-";

    setPositionStatus("正在获取精确位置...");

    gpsProcess->start("powershell.exe", args);
    gpsProcess->write(psScript);
    gpsProcess->closeWriteChannel();

#else
    // Linux及其他平台：优先使用GPS定位，失败时回退到IP定位
    setPositionStatus("正在获取GPS定位...");
    startLinuxGps();
#endif
}

/**
 * @brief 处理PowerShell定位脚本输出
 * @details 解析PowerShell脚本输出的经纬度数据，更新界面和地图
 *          如果GPS定位失败，回退到高德IP定位
 */
void Widget::onGpsLocationReceived()
{
    // 读取输出并去除BOM头和多余空白
    QByteArray rawOutput = gpsProcess->readAllStandardOutput();
    // 去除UTF-8 BOM头 (EF BB BF)
    if (rawOutput.size() >= 3 && (unsigned char)rawOutput[0] == 0xEF && (unsigned char)rawOutput[1] == 0xBB && (unsigned char)rawOutput[2] == 0xBF) {
        rawOutput = rawOutput.mid(3);
    }
    QString output = QString::fromUtf8(rawOutput).trimmed();
    qDebug() << "[GPS定位] PowerShell输出:" << output;

    // 查找 RESULT: 行（可能在多行输出中）
    int resultIdx = output.indexOf("RESULT:");
    if (resultIdx >= 0) {
        QString coordStr = output.mid(resultIdx + 7);  // 去掉 "RESULT:" 前缀
        // 取第一行
        int lineEnd = coordStr.indexOf('\n');
        if (lineEnd >= 0) {
            coordStr = coordStr.left(lineEnd);
        }
        coordStr = coordStr.trimmed();
        QStringList coords = coordStr.split(",");
        if (coords.size() == 2) {
            // WinRT返回的是WGS84坐标，需要转换为高德GCJ02坐标
            double wgsLng = coords[0].toDouble();
            double wgsLat = coords[1].toDouble();

            if (wgsLng != 0.0 && wgsLat != 0.0) {
                qDebug() << "[GPS定位] WGS84坐标:" << wgsLng << wgsLat << "正在转换为GCJ02...";
                // 使用高德坐标转换API将WGS84转为GCJ02
                QString key = "7e7bd3381de08c627a637eba7c9a29ec";
                QString urlStr = QString("http://restapi.amap.com/v3/assistant/coordinate/convert?locations=%1,%2&coordsys=gps&key=%3")
                                 .arg(wgsLng, 0, 'f', 6)
                                 .arg(wgsLat, 0, 'f', 6)
                                 .arg(key);

                QUrl url(urlStr);
                QNetworkRequest request(url);
                QNetworkReply *reply = networkManager->get(request);
                connect(reply, &QNetworkReply::finished, this, [=]() {
                    if (reply->error() == QNetworkReply::NoError) {
                        QByteArray data = reply->readAll();
                        qDebug() << "[GPS定位] 坐标转换响应:" << data;
                        QJsonDocument jsonDoc = QJsonDocument::fromJson(data);
                        qDebug() << "[GPS定位] JSON解析是否成功:" << !jsonDoc.isNull();
                        qDebug() << "[GPS定位] JSON是否为对象:" << jsonDoc.isObject();
                        if (!jsonDoc.isNull() && jsonDoc.isObject()) {
                            QJsonObject jsonObj = jsonDoc.object();
                            qDebug() << "[GPS定位] status字段值:" << jsonObj["status"];
                            if (jsonObj["status"].toString() == "1") {
                                qDebug() << "[GPS定位] locations字段类型:" << jsonObj["locations"].type();
                                QString locations = jsonObj["locations"].toString();
                                qDebug() << "[GPS定位] locations原始值:" << locations;
                                qDebug() << "[GPS定位] locations长度:" << locations.length();
                                
                                locations = locations.replace("\"", "");
                                locations = locations.trimmed();
                                qDebug() << "[GPS定位] locations去除引号后:" << locations;
                                
                                QStringList gcjCoords = locations.split(",");
                                qDebug() << "[GPS定位] 分割后数组大小:" << gcjCoords.size();
                                if (gcjCoords.size() == 2) {
                                    QString lonStr = gcjCoords[0].trimmed();
                                    QString latStr = gcjCoords[1].trimmed();
                                    qDebug() << "[GPS定位] 经度字符串:" << lonStr << ", 纬度字符串:" << latStr;
                                    double oldLon = longitude;
                                    double oldLat = latitude;
                                    longitude = lonStr.toDouble();
                                    latitude = latStr.toDouble();
                                    qDebug() << "[GPS定位] GCJ02坐标更新: 旧坐标(" << oldLon << "," << oldLat << ") → 新坐标(" << longitude << "," << latitude << ")";

                                    // 更新界面显示
                                    lblCoordinates->setText("纬度: " + QString::number(latitude, 'f', 6) + "°N\n" + "经度: " + QString::number(longitude, 'f', 6) + "°E");
                                    // 在位置信息框中加载地图图片（不显示经纬度文字）
                                    loadMapToPositionFrame(longitude, latitude);

                                    // 强制更新地图弹窗，无论是否可见
                                    if (mapDialog) {
                                        mapDialog->setPosition(longitude, latitude);
                                    }

                                    // 调用逆地理编码获取城市名
                                    QString regeoUrlStr = QString("http://restapi.amap.com/v3/geocode/regeo?location=%1,%2&key=%3")
                                                     .arg(longitude, 0, 'f', 6)
                                                     .arg(latitude, 0, 'f', 6)
                                                     .arg(key);
                                    QUrl regeoUrl(regeoUrlStr);
                                    QNetworkRequest regeoRequest(regeoUrl);
                                    QNetworkReply *regeoReply = networkManager->get(regeoRequest);
                                    connect(regeoReply, &QNetworkReply::finished, this, [=]() {
                                        if (regeoReply->error() == QNetworkReply::NoError) {
                                            QByteArray regeoData = regeoReply->readAll();
                                            QJsonDocument regeoDoc = QJsonDocument::fromJson(regeoData);
                                            if (!regeoDoc.isNull() && regeoDoc.isObject()) {
                                                QJsonObject regeoObj = regeoDoc.object();
                                                if (regeoObj["status"].toString() == "1") {
                                                    QJsonObject regeocode = regeoObj["regeocode"].toObject();
                                                    QJsonObject addressComponent = regeocode["addressComponent"].toObject();
                                                    QString city = addressComponent["city"].toString();
                                                    if (city.isEmpty()) {
                                                        // 直辖市可能city为空，用province
                                                        city = addressComponent["province"].toString();
                                                    }
                                                    if (!city.isEmpty()) {
                                                        currentCity = city;
                                                        qDebug() << "[GPS定位] 当前城市:" << currentCity;
                                                    }
                                                }
                                            }
                                        }
                                        regeoReply->deleteLater();
                                    });
                                }
                            }
                        }
                    } else {
                        qDebug() << "[GPS定位] 坐标转换失败:" << reply->errorString();
                    }
                    reply->deleteLater();
                });
                return;
            }
        }
    }

    // GPS定位失败，回退到高德IP定位
    requestIPLocation();
}

/**
 * @brief 启动Linux GPS定位
 * @details 使用Qt Positioning模块获取GPS定位，支持系统级GPSD或其他GPS后端
 *          如果GPS不可用或超时，回退到IP定位
 */
void Widget::startLinuxGps()
{
    qDebug() << "[LinuxGPS] 尝试启动GPS定位...";

    // 获取默认的定位源（优先使用GPS）
    positionSource = QGeoPositionInfoSource::createDefaultSource(this);
    if (!positionSource) {
        qWarning() << "[LinuxGPS] 无可用的定位源，回退到IP定位";
        setPositionStatus("GPS不可用，使用IP定位...");
        requestIPLocation();
        return;
    }

    // 设置GPS更新间隔和超时
    positionSource->setUpdateInterval(3000);
    positionSource->setPreferredPositioningMethods(
        QGeoPositionInfoSource::SatellitePositioningMethods);

    connect(positionSource, &QGeoPositionInfoSource::positionUpdated,
            this, &Widget::onLinuxGpsPositionUpdated);
    connect(positionSource, QOverload<QGeoPositionInfoSource::Error>::of(
                &QGeoPositionInfoSource::error),
            this, &Widget::onLinuxGpsError);

    // 启动定位
    positionSource->startUpdates();
    gpsActive = true;

    // 5秒超时：如果GPS未返回位置，回退到IP定位
    QTimer::singleShot(5000, this, [this]() {
        if (!usingGps && gpsActive) {
            qWarning() << "[LinuxGPS] GPS超时，回退到IP定位";
            setPositionStatus("GPS超时，使用IP定位...");
            positionSource->stopUpdates();
            gpsActive = false;
            requestIPLocation();
        }
    });

    qDebug() << "[LinuxGPS] GPS定位已启动";
}

/**
 * @brief 处理Linux GPS位置更新
 */
void Widget::onLinuxGpsPositionUpdated(const QGeoPositionInfo &info)
{
    if (!info.isValid()) return;

    QGeoCoordinate coord = info.coordinate();
    double gpsLon = coord.longitude();
    double gpsLat = coord.latitude();
    double accuracy = -1;
    if (info.hasAttribute(QGeoPositionInfo::HorizontalAccuracy))
        accuracy = info.attribute(QGeoPositionInfo::HorizontalAccuracy);

    if (gpsLon == 0.0 && gpsLat == 0.0) return;

    qDebug() << "[LinuxGPS] GPS定位成功:" << gpsLon << gpsLat << "精度:" << accuracy << "m";

    // GPS坐标是WGS-84，需要转换为GCJ-02用于高德地图
    // 使用高德坐标转换API
    QString key = "7e7bd3381de08c627a637eba7c9a29ec";
    QString urlStr = QString("http://restapi.amap.com/v3/assistant/coordinate/convert?locations=%1,%2&coordsys=gps&key=%3")
                     .arg(gpsLon, 0, 'f', 6)
                     .arg(gpsLat, 0, 'f', 6)
                     .arg(key);

    QUrl url(urlStr);
    QNetworkRequest request(url);
    QNetworkReply *reply = networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, [=]() {
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray data = reply->readAll();
            qDebug() << "[LinuxGPS] 坐标转换响应:" << data;
            QJsonDocument jsonDoc = QJsonDocument::fromJson(data);
            if (!jsonDoc.isNull() && jsonDoc.isObject()) {
                QJsonObject jsonObj = jsonDoc.object();
                if (jsonObj["status"].toString() == "1") {
                    QString locations = jsonObj["locations"].toString();
                    QStringList gcjCoords = locations.split(",");
                    if (gcjCoords.size() == 2) {
                        double oldLon = longitude;
                        double oldLat = latitude;
                        longitude = gcjCoords[0].toDouble();
                        latitude = gcjCoords[1].toDouble();
                        qDebug() << "[LinuxGPS] WGS84→GCJ02: (" << oldLon << "," << oldLat << ") → (" << longitude << "," << latitude << ")";

                        usingGps = true;
                        gpsActive = false;
                        if (positionSource) positionSource->stopUpdates();

                        // 更新界面
                        lblCoordinates->setText("纬度: " + QString::number(latitude, 'f', 6) + "°N\n" + "经度: " + QString::number(longitude, 'f', 6) + "°E");
                        setPositionStatus("GPS定位成功（精度" + QString::number(accuracy, 'f', 1) + "m）");

                        // 更新小地图和弹窗地图
                        loadMapToPositionFrame(longitude, latitude);
                        if (mapDialog) {
                            mapDialog->setPosition(longitude, latitude);
                        }
                        return;
                    }
                }
            }
        }
        // 转换失败，使用原始GPS坐标（WGS-84直接用在高德上会有200-500m偏移，但比IP定位好）
        qWarning() << "[LinuxGPS] 坐标转换失败，直接使用WGS84坐标";
        longitude = gpsLon;
        latitude = gpsLat;
        usingGps = true;
        gpsActive = false;
        if (positionSource) positionSource->stopUpdates();

        lblCoordinates->setText("纬度: " + QString::number(latitude, 'f', 6) + "°N\n" + "经度: " + QString::number(longitude, 'f', 6) + "°E");
        setPositionStatus("GPS定位成功（未转换）");
        loadMapToPositionFrame(longitude, latitude);
        if (mapDialog) {
            mapDialog->setPosition(longitude, latitude);
        }
        reply->deleteLater();
    });
}

/**
 * @brief 处理Linux GPS定位错误
 */
void Widget::onLinuxGpsError(QGeoPositionInfoSource::Error error)
{
    if (error == QGeoPositionInfoSource::AccessError) {
        qWarning() << "[LinuxGPS] GPS错误:" << error << "，回退到IP定位";
        if (!usingGps) {
            setPositionStatus("GPS不可用，使用IP定位...");
            gpsActive = false;
            if (positionSource) positionSource->stopUpdates();
            requestIPLocation();
        }
    }
}

/**
 * @brief 通过高德IP定位API获取位置（回退方案）
 * @details 当GPS定位失败时，使用高德地图IP定位接口获取城市级别位置
 */
void Widget::requestIPLocation()
{
    QString key = "7e7bd3381de08c627a637eba7c9a29ec";
    QString urlStr = QString("http://restapi.amap.com/v3/ip?key=%1").arg(key);

    QUrl url(urlStr);
    QNetworkRequest request(url);

    QNetworkReply *reply = networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, [=]() {
        onIpLocationResponse(reply);
    });
}

/**
 * @brief 处理IP定位响应
 * @details 解析高德IP定位API返回的JSON数据，提取经纬度并更新界面
 *          高德IP定位返回rectangle字段为矩形范围"minLng,minLat;maxLng,maxLat"
 *          取矩形中心点作为当前位置
 */
void Widget::onIpLocationResponse(QNetworkReply *reply)
{
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray data = reply->readAll();
        QJsonDocument jsonDoc = QJsonDocument::fromJson(data);

        if (!jsonDoc.isNull() && jsonDoc.isObject()) {
            QJsonObject jsonObj = jsonDoc.object();

            // status是字符串类型，不是数字
            if (jsonObj["status"].toString() == "1") {
                // 解析城市名
                QString city = jsonObj["city"].toString();
                if (!city.isEmpty()) {
                    currentCity = city;
                }

                QString rectangle = jsonObj["rectangle"].toString();
                if (!rectangle.isEmpty()) {
                    // rectangle格式: "minLng,minLat;maxLng,maxLat"
                    QStringList points = rectangle.split(";");
                    if (points.size() == 2) {
                        QStringList minCoord = points[0].split(",");
                        QStringList maxCoord = points[1].split(",");
                        if (minCoord.size() == 2 && maxCoord.size() == 2) {
                            double minLng = minCoord[0].toDouble();
                            double minLat = minCoord[1].toDouble();
                            double maxLng = maxCoord[0].toDouble();
                            double maxLat = maxCoord[1].toDouble();

                            // 取矩形中心点作为当前位置
                            longitude = (minLng + maxLng) / 2.0;
                            latitude = (minLat + maxLat) / 2.0;

                            // 更新界面显示
                            lblCoordinates->setText("纬度: " + QString::number(latitude, 'f', 6) + "°N\n" + "经度: " + QString::number(longitude, 'f', 6) + "°E");
                            // 在位置信息框中加载地图图片（不显示经纬度文字）
                            loadMapToPositionFrame(longitude, latitude);

                            // 如果地图弹窗已打开，更新地图显示
                            if (mapDialog && mapDialog->isVisible()) {
                                mapDialog->setPosition(longitude, latitude);
                            }
                        }
                    }
                }
            }
        }
    }
    reply->deleteLater();
}

/**
 * @brief 更新时间显示函数
 * @details 获取当前系统时间并更新界面显示，格式为：日期 上午/下午: 时间PM\n农历年 建国周年 黄帝纪年
 */
void Widget::updateTime()
{
    // 强制使用中国标准时间（东八区 Asia/Shanghai），避免系统时区非东八区时
    // 显示与国内实际时间不符（如开发机时区为 PDT 时会相差15小时且日期错位）
    QDateTime currentTime = QDateTime::currentDateTimeUtc().toTimeZone(QTimeZone("Asia/Shanghai"));

    QString dateStr = currentTime.toString("yyyy.MM.dd");
    QString timeStr = currentTime.toString("HH:mm");
    QString periodStr = currentTime.time().hour() >= 12 ? "下午" : "上午";
    
    int year = currentTime.date().year();
    
    QString ganzhiYear = getGanZhiYear(year);
    
    int nationAnniversary = year - 1949;
    int huangDiYear = year + 2698;
    
    QString timeText = QString("%1 %2: %3PM\n%4年 %5周年 黄帝纪年%6年")
                       .arg(dateStr)
                       .arg(periodStr)
                       .arg(timeStr)
                       .arg(ganzhiYear)
                       .arg(nationAnniversary)
                       .arg(huangDiYear);
    
    lblTime->setText(timeText);
}

/**
 * @brief 获取天干地支年份
 * @details 根据公历年份计算对应的天干地支年份
 */
QString Widget::getGanZhiYear(int year)
{
    QString tianGan[] = {"甲", "乙", "丙", "丁", "戊", "己", "庚", "辛", "壬", "癸"};
    QString diZhi[] = {"子", "丑", "寅", "卯", "辰", "巳", "午", "未", "申", "酉", "戌", "亥"};
    
    int ganIndex = (year - 4) % 10;
    int zhiIndex = (year - 4) % 12;
    
    if (ganIndex < 0) ganIndex += 10;
    if (zhiIndex < 0) zhiIndex += 12;
    
    return tianGan[ganIndex] + diZhi[zhiIndex];
}



/**
 * @brief 目的地输入框回车事件处理
 * @details 获取输入的目的地地址，通过百度地图API获取坐标并计算距离
 */
void Widget::onDestinationEntered()
{
    QString destination = leDestination->text().trimmed();
    if (destination.isEmpty()) {
        QMessageBox::warning(this, "提示", "请输入目的地地址");
        return;
    }
    
    QString key = "cefc60d5aadab5e043ec252c6239ae21";
    QString securityKey = "e2586e9a409674cde96dc8a1b418283e";
    QString encodedDest = QString::fromUtf8(QUrl::toPercentEncoding(destination));
    
    // 构建URL（不含sig参数）
    QString urlStr = QString("http://restapi.amap.com/v3/geocode/geo?address=%1&output=json&key=%2")
                     .arg(encodedDest)
                     .arg(key);
    
    // 计算签名
    QString sig = calculateSignature(urlStr, securityKey);
    
    // 添加签名到URL
    urlStr += QString("&sig=%1").arg(sig);
    
    QUrl qurl(urlStr);
    QNetworkRequest request(qurl);
    
    QNetworkReply *reply = networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, [=]() {
        onGeocodeResponse(reply);
    });
}

/**
 * @brief 处理目的地地理编码响应
 * @details 解析高德地图地理编码API返回的JSON数据
 */
void Widget::onGeocodeResponse(QNetworkReply *reply)
{
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray data = reply->readAll();
        QJsonDocument jsonDoc = QJsonDocument::fromJson(data);
        
        if (!jsonDoc.isNull() && jsonDoc.isObject()) {
            QJsonObject jsonObj = jsonDoc.object();
            int status = jsonObj["status"].toInt();
            
            if (status == 1) {
                QJsonArray geocodes = jsonObj["geocodes"].toArray();
                if (!geocodes.isEmpty()) {
                    QJsonObject result = geocodes[0].toObject();
                    QString locationStr = result["location"].toString();
                    
                    QStringList coords = locationStr.split(",");
                    if (coords.size() == 2) {
                        double destLon = coords[0].toDouble();
                        double destLat = coords[1].toDouble();
                        
                        if (destLat != 0 && destLon != 0) {
                            double distance = calculateDistance(latitude, longitude, destLat, destLon);
                            // 注意：totalDistance不重置，保持累计总行驶距离
                            traveledDistance = 0.0;
                            destinationDistance = distance;
                            lblTotalDistance->setText("总行驶距离：" + QString::number(totalDistance, 'f', 2) + "km");
                            lblTraveledDistance->setText("已行驶距离：" + QString::number(traveledDistance, 'f', 2) + "km");
                            lblDistance->setText("目的地距离：" + QString::number(destinationDistance, 'f', 2) + "km");
                            
                            QMessageBox::information(this, "计算结果", QString("到目的地的距离为：%1 km").arg(distance, 0, 'f', 2));
                        } else {
                            QMessageBox::warning(this, "提示", "未能获取目的地坐标");
                        }
                    } else {
                        QMessageBox::warning(this, "提示", "解析坐标数据失败");
                    }
                } else {
                    QMessageBox::warning(this, "提示", "未找到匹配的地址");
                }
            } else {
                QString info = jsonObj["info"].toString();
                QMessageBox::warning(this, "提示", "地理编码失败：" + info);
            }
        } else {
            QMessageBox::warning(this, "提示", "解析地理编码数据失败");
        }
    } else {
        QMessageBox::warning(this, "提示", "网络请求失败，请检查网络连接");
    }
    reply->deleteLater();
}

/**
 * @brief 显示事件
 * @details 窗口显示后，布局计算完成，此时用QTimer延迟将指南针左移20px
 *          （QBoxLayout对setFixedSize控件的QSS margin不生效，故用move直接调整）
 */
void Widget::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    // 延迟到事件循环，确保布局已完成几何计算后再移动
    QTimer::singleShot(0, this, [this]() {
        if (compassWidget) {
            compassWidget->move(compassWidget->x() - 40, compassWidget->y());
        }
        // 首次显示时加载位置信息框动态地图
        if (positionWebView && !positionMapLoaded) {
            positionMapLoaded = true;
            initPositionMap();
        }
        // 同步容器几何位置（多次延迟同步确保布局稳定后对齐）
        syncPositionMapGeometry();
        QTimer::singleShot(100, this, [this]() { syncPositionMapGeometry(); });
        QTimer::singleShot(300, this, [this]() { syncPositionMapGeometry(); });
        // 若电源已开 且 地图弹窗未显示，则显示顶层窗口并提升层级
        if (positionWebView && isPowerOn && positionWebView->isHidden()
            && (!mapDialog || !mapDialog->isVisible())) {
            positionWebView->show();
            positionWebView->raise();
        }
    });
}

/**
 * @brief 隐藏事件
 * @details 主窗口被隐藏/最小化时，同步隐藏 positionWebView 子窗口
 */
void Widget::hideEvent(QHideEvent *event)
{
    QWidget::hideEvent(event);
    // 主窗口被隐藏（包括最小化），同步隐藏地图子窗口
    if (positionWebView && !positionWebView->isHidden()) {
        positionWebView->hide();
    }
}

/**
 * @brief 移动事件
 * @details 主窗口移动时同步位置信息框地图子窗口的位置，
 *          确保地图始终覆盖在占位控件上方
 */
void Widget::moveEvent(QMoveEvent *event)
{
    QWidget::moveEvent(event);
    // 主窗口移动后，子窗口位置需要重新同步
    syncPositionMapGeometry();
}

/**
 * @brief 尺寸变化事件
 * @details 主窗口尺寸变化时，同步位置信息框地图子窗口的位置和大小
 */
void Widget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    syncPositionMapGeometry();
}

/**
 * @brief 窗口状态变化事件处理
 * @param event 事件对象
 * @details positionWebView 现为 mapFrame 子窗口，主窗口最小化/恢复时自动跟随，
 *          无需独立处理。此处保留电源开关逻辑确保地图按需显示。
 */
void Widget::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::WindowStateChange) {
        if (!positionWebView) return;
        if (isMinimized()) {
            positionWebView->hide();
        } else if (!isHidden()) {
            // 主窗口从最小化恢复时，若位置地图本应可见则重新显示
            if (isPowerOn && positionMapReady && !positionWebView->isVisible()
                && (!mapDialog || !mapDialog->isVisible())) {
                positionWebView->show();
                syncPositionMapGeometry();
            }
        }
    }
}

/**
 * @brief 事件过滤器
 * @details 用于捕获目的地输入框的焦点事件
 * @param obj 事件对象
 * @param event 事件
 * @return 是否处理了事件
 */
bool Widget::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == leDestination) {
        if (event->type() == QEvent::MouseButtonPress) {
            // 鼠标点击输入框时显示小地图
            showMiniMap();
            // 返回false让事件继续传播，确保输入框能正常获得焦点
            return false;
        } else if (event->type() == QEvent::KeyPress) {
            // 检测回车键
            QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
            if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
                // 回车键触发立即搜索目的地
                QString destination = leDestination->text().trimmed();
                if (!destination.isEmpty()) {
                    // 停止防抖定时器，立即执行查询
                    destSearchTimer->stop();
                    onDestSearchTimeout();
                    qDebug() << "[小地图] 回车键触发，立即查询目的地";
                }
                // 返回true阻止默认行为（如换行）
                return true;
            }
        }
    }
    // 捕获占位控件的移动/尺寸变化事件，即时同步地图顶层子窗口几何位置
    // 避免主窗口布局调整时地图子窗口滞后导致"飘出"信息框
    if (obj == positionMapPlaceholder) {
        if (event->type() == QEvent::Move || event->type() == QEvent::Resize ||
            event->type() == QEvent::ShowToParent) {
            syncPositionMapGeometry();
        }
        return false;  // 不拦截事件，继续传播
    }
    return QWidget::eventFilter(obj, event);
}

/**
 * @brief 显示小地图窗口
 * @details 当用户点击目的地输入框时，显示小地图窗口
 */
void Widget::showMiniMap()
{
    // 获取当前经纬度，若无效（未定位的初始值 0,0 或 NaN）则回退到默认成都坐标，
    // 避免 (0,0) 加载几内亚湾海面空白瓦片导致地图无内容
    double lon = longitude;
    double lat = latitude;
    if (qIsNaN(lon) || qIsNaN(lat) || (lon == 0.0 && lat == 0.0)) {
        lon = 104.0668;  // 默认成都经度
        lat = 30.5728;   // 默认成都纬度
    }

    if (!miniMapDialog) {
        miniMapDialog = new MiniMapDialog(this);
        miniMapDialog->setPosition(lon, lat);
        qDebug() << "[小地图] 创建小地图并加载位置:" << lon << lat;
    } else {
        // 已存在则刷新到最新有效位置，避免残留旧的空白/海洋地图
        miniMapDialog->updatePosition(lon, lat);
    }

    // 显示小地图（如果已经打开，不再重新加载地图）
    miniMapDialog->show();
    // 强制提升小地图Z序到最前，确保浮在 positionWebView 及其他控件之上
    miniMapDialog->raise();
    miniMapDialog->activateWindow();

    // 将小地图窗口定位到输入框下方
    QPoint globalPos = leDestination->mapToGlobal(QPoint(0, leDestination->height()));
    miniMapDialog->move(globalPos.x(), globalPos.y() + 5);
    
    qDebug() << "[小地图] 显示小地图窗口";
}

/**
 * @brief 隐藏小地图窗口
 * @details 当用户点击其他地方时，隐藏小地图窗口
 */
void Widget::hideMiniMap()
{
    if (miniMapDialog && miniMapDialog->isVisible()) {
        miniMapDialog->hide();
        qDebug() << "[小地图] 鼠标点击其他地方，隐藏地图窗口";
    }
}

/**
 * @brief 重写鼠标按下事件
 * @details 检测鼠标点击是否在输入框或小地图之外，如果是则关闭小地图
 *          位置信息框动态地图的拖动由 QWebEngineView（高德地图 JS API）原生处理
 */
void Widget::mousePressEvent(QMouseEvent *event)
{
    // 调用父类处理
    QWidget::mousePressEvent(event);

    // 获取点击位置的全局坐标
    QPoint globalPos = this->mapToGlobal(event->pos());

    // 检查点击是否在输入框范围内
    bool clickedOnInput = leDestination->geometry().contains(leDestination->mapFromGlobal(globalPos));

    // 检查点击是否在小地图范围内
    bool clickedOnMiniMap = false;
    if (miniMapDialog && miniMapDialog->isVisible()) {
        clickedOnMiniMap = miniMapDialog->geometry().contains(miniMapDialog->mapFromGlobal(globalPos));
    }

    // 如果点击不在输入框和小地图上，则关闭小地图
    if (!clickedOnInput && !clickedOnMiniMap) {
        hideMiniMap();
    }
}

/**
 * @brief 重写鼠标移动事件
 * @details 位置信息框动态地图的拖动由高德地图 JS API 原生处理，此处直接交由父类
 */
void Widget::mouseMoveEvent(QMouseEvent *event)
{
    QWidget::mouseMoveEvent(event);
}

/**
 * @brief 重写鼠标释放事件
 * @details 位置信息框动态地图的拖动结束由高德地图 JS API 原生处理
 */
void Widget::mouseReleaseEvent(QMouseEvent *event)
{
    QWidget::mouseReleaseEvent(event);
}

/**
 * @brief 重写鼠标双击事件
 * @details 位置信息框动态地图的双击恢复跟随由高德地图 JS API 原生处理（双击地图恢复跟随模式）
 */
void Widget::mouseDoubleClickEvent(QMouseEvent *event)
{
    QWidget::mouseDoubleClickEvent(event);
}

/**
 * @brief 处理目的地输入框文本变化
 * @details 当用户在目的地输入框中输入时触发，使用防抖机制避免频繁网络请求
 * @param text 当前输入框文本
 */
void Widget::onDestinationTextChanged(const QString &text)
{
    QString trimmedText = text.trimmed();
    if (trimmedText.isEmpty()) {
        return;
    }
    
    // 停止之前的定时器，重新开始计时（防抖）
    destSearchTimer->stop();
    destSearchTimer->start();
}

/**
 * @brief 目的地搜索防抖定时器触发处理
 * @details 当用户输入停止500ms后执行地理编码查询，并更新地图弹窗
 */
void Widget::onDestSearchTimeout()
{
    QString destination = leDestination->text().trimmed();
    if (destination.isEmpty()) {
        return;
    }
    
    // 使用高德地理编码API获取目的地坐标，限定在当前城市
    QString key = "7e7bd3381de08c627a637eba7c9a29ec";
    QString encodedDest = QString::fromUtf8(QUrl::toPercentEncoding(destination));
    QString cityForSearch = currentCity.isEmpty() ? "成都" : currentCity;
    QString encodedCity = QString::fromUtf8(QUrl::toPercentEncoding(cityForSearch));

    QString urlStr = QString("http://restapi.amap.com/v3/geocode/geo?address=%1&city=%2&output=json&key=%3")
                     .arg(encodedDest)
                     .arg(encodedCity)
                     .arg(key);
    
    QUrl qurl(urlStr);
    QNetworkRequest request(qurl);
    
    QNetworkReply *reply = networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, [=]() {
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray data = reply->readAll();
            QJsonDocument jsonDoc = QJsonDocument::fromJson(data);
            
            if (!jsonDoc.isNull() && jsonDoc.isObject()) {
                QJsonObject jsonObj = jsonDoc.object();
                if (jsonObj["status"].toString() == "1") {
                    QJsonArray geocodes = jsonObj["geocodes"].toArray();
                    if (!geocodes.isEmpty()) {
                        QJsonObject result = geocodes[0].toObject();
                        QString locationStr = result["location"].toString();
                        QStringList coords = locationStr.split(",");
                        if (coords.size() == 2) {
                            destLongitude = coords[0].toDouble();
                            destLatitude = coords[1].toDouble();
                            
                            if (destLatitude != 0 && destLongitude != 0) {
                                // 计算距离并更新显示
                                double distance = calculateDistance(latitude, longitude, destLatitude, destLongitude);
                                // 注意：totalDistance不重置，保持累计总行驶距离
                                traveledDistance = 0.0;
                                destinationDistance = distance;
                                lblTotalDistance->setText("总行驶距离：" + QString::number(totalDistance, 'f', 2) + "km");
                                lblTraveledDistance->setText("已行驶距离：" + QString::number(traveledDistance, 'f', 2) + "km");
                                lblDistance->setText("目的地距离：" + QString::number(destinationDistance, 'f', 2) + "km");
                                
                                qDebug() << "[实时匹配] 目的地:" << destination << "坐标:" << destLongitude << "," << destLatitude << "距离:" << distance << "km";
                                
                                // 如果小地图窗口已打开，更新地图显示目的地位置（居中显示目的地真实位置）
                                if (miniMapDialog && miniMapDialog->isVisible()) {
                                    miniMapDialog->setDestination(destLongitude, destLatitude);
                                    miniMapDialog->updatePosition(destLongitude, destLatitude);
                                }
                                
                                // 如果地图弹窗已打开，更新路线显示
                                if (mapDialog && mapDialog->isVisible()) {
                                    mapDialog->setRoute(longitude, latitude, destLongitude, destLatitude);
                                }
                            }
                        }
                    }
                }
            }
        }
        reply->deleteLater();
    });
}

/**
 * @brief 计算两点之间的距离（Haversine公式）
 * @details 根据经纬度计算两个位置之间的距离
 * @param lat1 起点纬度
 * @param lon1 起点经度
 * @param lat2 终点纬度
 * @param lon2 终点经度
 * @return 距离（公里）
 */
double Widget::calculateDistance(double lat1, double lon1, double lat2, double lon2)
{
    const double R = 6371.0;
    
    double lat1Rad = lat1 * M_PI / 180.0;
    double lon1Rad = lon1 * M_PI / 180.0;
    double lat2Rad = lat2 * M_PI / 180.0;
    double lon2Rad = lon2 * M_PI / 180.0;
    
    double dLat = lat2Rad - lat1Rad;
    double dLon = lon2Rad - lon1Rad;
    
    double a = sin(dLat / 2) * sin(dLat / 2) +
               cos(lat1Rad) * cos(lat2Rad) *
               sin(dLon / 2) * sin(dLon / 2);
    double c = 2 * atan2(sqrt(a), sqrt(1 - a));
    
    return R * c;
}

/**
 * @brief 计算高德地图API签名（HMAC-SHA256）
 * @param url 完整URL（不含sig参数）
 * @param securityKey 安全密钥
 * @return 签名字符串
 */
QString Widget::calculateSignature(const QString &url, const QString &securityKey)
{
    // 提取路径和参数
    QUrl qurl(url);
    QString path = qurl.path();
    QString query = qurl.query();
    
    // 按参数名排序
    QStringList params = query.split("&");
    qSort(params);
    
    // 拼接排序后的参数
    QString sortedParams = params.join("&");
    
    // 构建待签名字符串：/path?key=xxx&param1=xxx&param2=xxx
    QString signStr = path + "?" + sortedParams;
    
    // 使用HMAC-SHA256计算签名
    QByteArray keyBytes = securityKey.toUtf8();
    QByteArray dataBytes = signStr.toUtf8();
    
    // 计算HMAC-SHA256
    QByteArray hmac = hmacSha256(dataBytes, keyBytes);
    
    // 转换为十六进制字符串
    QString sig = QString(hmac.toHex());
    
    return sig;
}

/**
 * @brief HMAC-SHA256计算
 */
QByteArray Widget::hmacSha256(const QByteArray &data, const QByteArray &key)
{
    QByteArray blockSize = QByteArray(64, 0);
    QByteArray oKeyPad(key);
    QByteArray iKeyPad(key);
    
    if (key.size() > 64) {
        // 如果密钥长度大于64字节，先进行SHA256哈希
        QCryptographicHash hash(QCryptographicHash::Sha256);
        hash.addData(key);
        oKeyPad = hash.result();
        iKeyPad = oKeyPad;
    }
    
    // 补零到64字节
    if (oKeyPad.size() < 64) {
        oKeyPad.append(QByteArray(64 - oKeyPad.size(), 0));
        iKeyPad.append(QByteArray(64 - iKeyPad.size(), 0));
    }
    
    // XOR操作
    for (int i = 0; i < 64; i++) {
        oKeyPad[i] = oKeyPad[i] ^ 0x5C;
        iKeyPad[i] = iKeyPad[i] ^ 0x36;
    }
    
    // 计算内层哈希：SHA256(iKeyPad + data)
    QCryptographicHash innerHash(QCryptographicHash::Sha256);
    innerHash.addData(iKeyPad);
    innerHash.addData(data);
    QByteArray innerResult = innerHash.result();
    
    // 计算外层哈希：SHA256(oKeyPad + innerResult)
    QCryptographicHash outerHash(QCryptographicHash::Sha256);
    outerHash.addData(oKeyPad);
    outerHash.addData(innerResult);
    
    return outerHash.result();
}

/**
 * @brief 生成科技感图标
 * @details 使用QPixmap绘制科技风格的图标，与界面背景配色匹配
 * @param type 图标类型：0=高度, 1=速度, 2=时长, 3=坐标, 4=距离, 5=重量, 6=电池, 7=地图
 * @return QPixmap图标
 */
QPixmap Widget::createTechIcon(int type)
{
    QPixmap pixmap(24, 24);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    // 锐化高对比颜色（强太阳光下清晰可读）
    QColor cyan(0, 212, 255, 255);      // 青蓝主色（不透明）
    QColor green(0, 230, 118, 255);     // 绿色状态色
    QColor yellow(255, 214, 0, 255);    // 黄色警示色

    switch (type) {
    case 0: // 高度 - 向上箭头（加粗2.5px）
        painter.setPen(QPen(cyan, 2.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawLine(12, 20, 12, 6);
        painter.drawLine(12, 6, 6, 12);
        painter.drawLine(12, 6, 18, 12);
        break;
    case 1: // 速度 - 闪电（加粗2.5px，黄色）
        painter.setPen(QPen(yellow, 2.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawLine(6, 12, 18, 12);
        painter.drawLine(12, 6, 12, 18);
        painter.drawLine(6, 6, 18, 18);
        painter.drawLine(18, 6, 6, 18);
        break;
    case 2: // 时长 - 时钟（加粗2.5px）
        painter.setPen(QPen(cyan, 2.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawEllipse(4, 4, 16, 16);
        painter.drawLine(12, 8, 12, 12);
        painter.drawLine(12, 12, 14, 14);
        break;
    case 3: // 坐标 - 十字准星（加粗2.5px，绿色）
        painter.setPen(QPen(green, 2.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawLine(12, 2, 12, 22);
        painter.drawLine(2, 12, 22, 12);
        painter.drawEllipse(8, 8, 8, 8);
        break;
    case 4: // 距离 - 雷达波（加粗2px）
        painter.setPen(QPen(cyan, 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawEllipse(4, 4, 16, 16);
        painter.drawEllipse(8, 8, 8, 8);
        painter.drawEllipse(11, 11, 4, 4);
        painter.drawLine(12, 12, 12, 4);
        break;
    case 5: // 重量 - 秤/方块（加粗2.5px）
        painter.setPen(QPen(cyan, 2.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawRect(6, 8, 12, 12);
        painter.drawLine(6, 8, 12, 4);
        painter.drawLine(12, 4, 18, 8);
        break;
    case 6: // 电池 - 电池形状（加粗2.5px，黄色）
        painter.setPen(QPen(yellow, 2.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawRect(6, 6, 12, 12);
        painter.drawRect(18, 10, 2, 4);
        painter.fillRect(8, 8, 8, 8, QBrush(yellow, Qt::Dense4Pattern));
        break;
    case 7: // 地图 - 地图标记（加粗2.5px，绿色）
        painter.setPen(QPen(green, 2.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawLine(12, 22, 8, 14);
        painter.drawLine(8, 14, 16, 14);
        painter.drawLine(16, 14, 12, 22);
        painter.drawLine(12, 14, 12, 6);
        painter.drawLine(12, 6, 10, 8);
        painter.drawLine(12, 6, 14, 8);
        break;
    default:
        painter.setPen(QPen(cyan, 2.5));
        painter.drawRect(6, 6, 12, 12);
        break;
    }

    return pixmap;
}

// ============================================================================
// PX4 仿真集成实现
// ============================================================================

/**
 * @brief 初始化 PX4 控制器
 * @details 创建 Px4Controller 实例，连接信号槽，启动工作线程。
 *          不自动连接 PX4，由电源按钮触发连接。
 *          同时创建左下角 PX4 状态标签和 1Hz 遥测刷新定时器。
 */
void Widget::initPx4Controller()
{
    // 创建 PX4 控制器（无 parent，手动管理生命周期）
    px4Controller = new Px4Controller();
    px4Controller->start();  // 启动内部工作线程

    // 连接 PX4 控制器信号到本类槽
    connect(px4Controller, &Px4Controller::connectionStateChanged,
            this, &Widget::onPx4ConnectionStateChanged);
    connect(px4Controller, &Px4Controller::telemetryUpdated,
            this, &Widget::onPx4TelemetryUpdated);
    connect(px4Controller, &Px4Controller::operationResult,
            this, &Widget::onPx4OperationResult);
    connect(px4Controller, &Px4Controller::flightModeChanged,
            this, &Widget::onPx4FlightModeChanged);

    // 创建 PX4 状态显示标签（覆盖在主界面左下角）
    lblPx4Status = new QLabel(this);
    lblPx4Status->setObjectName("px4StatusLabel");
    lblPx4Status->setText("PX4: 未连接");
    lblPx4Status->setStyleSheet(
        "QLabel#px4StatusLabel { "
        "background: rgba(24, 30, 38, 0.9); "
        "color: #ff5252; "
        "border: 1px solid #ff5252; "
        "border-radius: 6px; "
        "padding: 4px 10px; "
        "font-size: 11px; font-weight: bold; "
        "}");
    lblPx4Status->setFixedSize(100, 24);
    lblPx4Status->move(12, height() - 500);
    lblPx4Status->raise();//将lblPx4Status控件提升到父窗口控件堆栈的顶部
    lblPx4Status->show();

    // PX4 遥测界面刷新定时器（1Hz，从快照刷新 UI）
    px4PollTimer = new QTimer(this);
    px4PollTimer->setInterval(1000);
    connect(px4PollTimer, &QTimer::timeout, this, &Widget::onPx4PollTimer);
    // 注意：不立即启动，电源开启 + PX4 连接成功后才启动
}

/**
 * @brief 处理 PX4 连接状态变化
 */
void Widget::onPx4ConnectionStateChanged(int state)
{
    Px4Controller::ConnectionState s = static_cast<Px4Controller::ConnectionState>(state);
    QString text;
    QString color;
    switch (s) {
    case Px4Controller::Disconnected:
        px4Connected = false;
        text = "PX4: 未连接";
        color = "#ff5252";
        if (px4PollTimer) px4PollTimer->stop();
        break;
    case Px4Controller::Connecting:
        text = "PX4: 连接中...";
        color = "#ffd600";
        break;
    case Px4Controller::Connected:
        px4Connected = true;
        text = "PX4: 已连接";
        color = "#00e676";
        if (px4PollTimer) px4PollTimer->start();
        qDebug() << "[Widget] PX4 已连接，启动遥测刷新";
        break;
    case Px4Controller::Disconnected_Error:
        px4Connected = false;
        text = "PX4: 连接失败";
        color = "#ff5252";
        if (px4PollTimer) px4PollTimer->stop();
        break;
    }

    if (lblPx4Status) {
        lblPx4Status->setText(text);
        lblPx4Status->setStyleSheet(
            QString("QLabel#px4StatusLabel { "
                    "background: rgba(24, 30, 38, 0.9); "
                    "color: %1; "
                    "border: 1px solid %1; "
                    "border-radius: 6px; "
                    "padding: 4px 10px; "
                    "font-size: 11px; font-weight: bold; }").arg(color));
    }
}

/**
 * @brief 处理 PX4 遥测数据更新（5Hz 由 Px4Controller 触发）
 * @details 实时更新内部飞行数据变量，但界面统一由 onPx4PollTimer 1Hz 刷新
 */
void Widget::onPx4TelemetryUpdated(const Px4Controller::TelemetrySnapshot &t)
{
    if (!t.valid) return;  // 遥测数据无效时直接返回，不更新任何变量

    // 用 PX4 真实遥测覆盖本地模拟数据
    latitude = t.latitudeDeg;                    // 更新纬度（度）
    longitude = t.longitudeDeg;                  // 更新经度（度）
    flightHeight = t.relativeAltM;               // 更新相对高度（米）——界面显示的高度数据来源
    flightSpeed = t.speedMS * 3.6;               // 更新速度：m/s → km/h（乘以 3.6）
    batteryLevel = t.batteryPct;                 // 更新电池电量百分比
    px4Armed = t.armed;                          // 更新解锁状态标志（用于关机/落地检测）
    px4FlightMode = static_cast<int>(t.mode);    // 更新当前飞行模式枚举值
    hoverHeading = t.yawDeg;                     // 更新航向角（度）

    // 记录无人机是否曾经升空（高度 > 1m）
    // 该标志用于降落检测：只有曾经升空过，才允许兜底逻辑判断"已落地"
    // 避免起飞过程中（高度还低时）被误判为已落地，导致 isFlying 被错误设为 false
    if (flightHeight > 1.0) {
        px4WasInAir = true;  // 标记无人机曾经升空
    }

    // 已行驶距离和距起飞点距离用 PX4 数据
    traveledDistance = t.totalDistKm;            // 更新当前飞行已行驶距离（km）
    // 若没有设置目的地，用距起飞点距离作为参考
    if (totalDistance <= 0.001 && destinationDistance <= 0.001) {
        destinationDistance = t.homeDistKm;
    }
}

/**
 * @brief 处理 PX4 操作结果反馈
 */
void Widget::onPx4OperationResult(const QString &operation, bool ok, const QString &msg)
{
    qDebug().noquote() << "[PX4 op]" << operation << (ok ? "OK" : "FAIL") << msg;

    // 失败的操作弹窗提示（仅关键操作）
    if (!ok) {
        if (operation == "connect") {
            QMessageBox::warning(this, "PX4 连接失败",
                "无法连接 PX4 SITL。\n" + msg +
                "\n\n请确认:\n1. 已运行 run_simulation.sh\n2. UDP 14540 端口可用");
        } else if (operation == "takeoff" || operation == "armAndTakeoff") {
            QMessageBox::warning(this, "起飞失败",
                "PX4 起飞指令失败。\n" + msg +
                "\n\n请确认无人机已解锁且处于可起飞状态");
        } else if (operation == "arm") {
            QMessageBox::warning(this, "解锁失败",
                "PX4 解锁失败。\n" + msg +
                "\n\n请确认无人机未处于飞行中且电池电量充足");
        }
    } else {
        // 成功的关键操作给出提示
        if (operation == "connect") {
            qDebug() << "[Widget] PX4 连接成功，可进行起飞操作";
        } else if (operation == "armAndTakeoff") {
            qDebug() << "[Widget] armAndTakeoff 成功";
        }
    }
}

/**
 * @brief 处理 PX4 飞行模式变化
 */
void Widget::onPx4FlightModeChanged(int mode)
{
    px4FlightMode = mode;
    Px4Controller::FlightMode fm = static_cast<Px4Controller::FlightMode>(mode);
    QString modeName;
    switch (fm) {
    case Px4Controller::Mode_Ground_Idle:    modeName = "地面待机"; break;
    case Px4Controller::Mode_Ground_Armed:   modeName = "已解锁待飞"; break;
    case Px4Controller::Mode_Takeoff:        modeName = "起飞中"; break;
    case Px4Controller::Mode_Hold:           modeName = "悬停"; break;
    case Px4Controller::Mode_Mission:        modeName = "任务飞行"; break;
    case Px4Controller::Mode_Return:         modeName = "返航"; break;
    case Px4Controller::Mode_Land:           modeName = "降落中"; break;
    case Px4Controller::Mode_Offboard:       modeName = "Offboard"; break;
    case Px4Controller::Mode_Manual:         modeName = "手动"; break;
    default:                                 modeName = "未知"; break;
    }

    QString defaultStyle = "width: 55px; height: 55px; border-radius: 27px; "
                           "border: 1.5px solid rgba(0, 212, 255, 0.7); border-top: 1.5px solid rgba(140, 215, 245, 0.9); border-left: 1.5px solid rgba(110, 200, 235, 0.85); border-right: 1.5px solid rgba(0, 80, 110, 0.9); border-bottom: 1.5px solid rgba(0, 55, 80, 0.95);"
                           "background: qradialgradient(cx:0.3, cy:0.25, radius:0.9, fx:0.3, fy:0.25, stop:0 rgba(82, 108, 138, 0.95), stop:0.4 rgba(42, 56, 72, 0.93), stop:0.8 rgba(20, 28, 38, 0.97), stop:1 rgba(6, 10, 16, 1)); "
                           "color: #00d4ff; font-size: 12px; font-weight: bold; text-align: center; "
                           "box-shadow: inset 0 1px 2px rgba(0, 212, 255, 0.25), inset 0 -2px 4px rgba(0,0,0,0.5), 0 0 10px rgba(0, 212, 255, 0.15), 0 3px 8px rgba(0,0,0,0.65);";

    // 同步本地飞行状态变量 + 更新按钮样式
    if (fm == Px4Controller::Mode_Takeoff) {
        isFlying = true;
        isLanding = false;
        isHovering = false;

        // 起飞中：起飞按钮蓝色，悬停/降落默认
        QString takeoffBlueStyle = "width: 55px; height: 55px; border-radius: 27px; "
                                   "border: 1.5px solid #1a8cff; border-top: 1.5px solid #66b0ff; border-left: 1.5px solid #4da6ff; border-right: 1.5px solid #0050aa; border-bottom: 1.5px solid #003580;"
                                   "background: qradialgradient(cx:0.35, cy:0.3, radius:0.85, fx:0.35, fy:0.3, stop:0 rgba(60, 140, 255, 0.55), stop:0.6 rgba(0, 80, 200, 0.9), stop:1 rgba(0, 35, 110, 0.98)); "
                                   "color: #4da6ff; font-size: 12px; font-weight: bold; text-align: center; "
                                   "box-shadow: inset 0 1px 2px rgba(100,180,255,0.3), inset 0 -2px 4px rgba(0,0,0,0.4), 0 0 20px rgba(0, 150, 255, 0.4), 0 3px 10px rgba(0,80,200,0.65);";
        btnTakeoff->setStyleSheet(takeoffBlueStyle);
        btnTakeoff->style()->unpolish(btnTakeoff);
        btnTakeoff->style()->polish(btnTakeoff);
        btnHover->setStyleSheet(defaultStyle);
        btnHover->style()->unpolish(btnHover);
        btnHover->style()->polish(btnHover);
        btnLand->setStyleSheet(defaultStyle);
        btnLand->style()->unpolish(btnLand);
        btnLand->style()->polish(btnLand);

    } else if (fm == Px4Controller::Mode_Hold) {
        // Hold 模式（悬停）：需要区分"真正悬停"和"降落后进入 Hold"
        // 判断是否实际已落地的条件：
        //   1. px4WasInAir = true（无人机曾经升空过，避免起飞前误判）
        //   2. flightHeight < 0.3（高度低于 0.3m）
        //   3. flightSpeed < 1.0（速度低于 1km/h）
        //   4. !px4Armed（无人机已上锁，PX4 降落后会自动上锁）
        bool actuallyOnGround = (px4WasInAir && flightHeight < 0.3 && flightSpeed < 1.0 && !px4Armed);  // 综合判断是否已落地

        if (actuallyOnGround) {
            // PX4 虽在 Hold 模式，但无人机实际已落地（降落后 PX4 可能短暂进入 Hold）
            isFlying = false;    // 重置飞行状态
            isLanding = false;   // 重置降落状态
            isHovering = false;  // 重置悬停状态

            // 所有按钮恢复默认样式
            btnHover->setStyleSheet(defaultStyle);                     // 悬停按钮恢复默认
            btnHover->style()->unpolish(btnHover);                     // 取消样式应用（清除旧样式）
            btnHover->style()->polish(btnHover);                       // 重新应用样式（刷新显示）
            btnTakeoff->setStyleSheet(defaultStyle);                   // 起飞按钮恢复默认
            btnTakeoff->style()->unpolish(btnTakeoff);                 // 取消样式应用
            btnTakeoff->style()->polish(btnTakeoff);                   // 重新应用样式
            btnLand->setStyleSheet(defaultStyle);                      // 降落按钮恢复默认
            btnLand->style()->unpolish(btnLand);                       // 取消样式应用
            btnLand->style()->polish(btnLand);                         // 重新应用样式

            qDebug() << "[Widget] Hold 模式但无人机已落地 (高度=" << flightHeight << "m)";  // 调试日志
        } else {
            // 真正悬停中（无人机在空中保持位置）
            isFlying = true;       // 标记为飞行中
            isLanding = false;     // 不是降落中
            isHovering = true;     // 标记为悬停中

            // 悬停中：悬停按钮红色（表示激活），起飞/降落按钮恢复默认
            QString hoverRedStyle = "width: 55px; height: 55px; border-radius: 27px; "       // 按钮尺寸和圆角
                                    "border: 1.5px solid #ff5252; border-top: 1.5px solid #ff9999; border-left: 1.5px solid #ff7777; border-right: 1.5px solid #aa2020; border-bottom: 1.5px solid #881515;"                              // 红色边框
                                    "background: qradialgradient(cx:0.35, cy:0.3, radius:0.85, fx:0.35, fy:0.3, stop:0 rgba(255, 100, 100, 0.55), stop:0.6 rgba(180, 30, 30, 0.9), stop:1 rgba(100, 15, 15, 0.98)); "  // 红色径向渐变背景
                                    "color: #ff5252; font-size: 12px; font-weight: bold; text-align: center; "  // 红色文字
                                    "box-shadow: inset 0 2px 4px rgba(255,255,255,0.25), 0 0 25px rgba(255, 100, 100, 0.5), 0 3px 8px rgba(150,0,0,0.7);";  // 发光阴影效果
            btnHover->setStyleSheet(hoverRedStyle);                   // 应用红色样式到悬停按钮
            btnHover->style()->unpolish(btnHover);                     // 刷新样式
            btnHover->style()->polish(btnHover);
            btnTakeoff->setStyleSheet(defaultStyle);                   // 起飞按钮恢复默认
            btnTakeoff->style()->unpolish(btnTakeoff);
            btnTakeoff->style()->polish(btnTakeoff);
            btnLand->setStyleSheet(defaultStyle);                      // 降落按钮恢复默认
            btnLand->style()->unpolish(btnLand);
            btnLand->style()->polish(btnLand);
        }

    } else if (fm == Px4Controller::Mode_Offboard) {
        // Offboard 模式：无人机正在执行 Offboard 指令（如高度调整）
        // 保持飞行状态为 true，不改变按钮样式（因为这只是临时模式，完成后会回到 Hold）
        isFlying = true;       // 仍在飞行中
        isLanding = false;     // 不是降落
        // 按钮样式保持当前状态不变（悬停按钮保持红色等）

    } else if (fm == Px4Controller::Mode_Land) {
        // Land 模式：无人机正在降落
        isLanding = true;      // 标记为降落中
        isFlying = true;       // 仍在飞行中（降落过程也算飞行）
        isHovering = false;    // 取消悬停状态

        // 降落中：降落按钮黄色，悬停/起飞默认
        QString landYellowStyle = "width: 55px; height: 55px; border-radius: 27px; "
                                   "border: 1.5px solid #ffd600; border-top: 1.5px solid #fff066; border-left: 1.5px solid #ffe633; border-right: 1.5px solid #aa8c00; border-bottom: 1.5px solid #886b00;"
                                   "background: qradialgradient(cx:0.35, cy:0.3, radius:0.85, fx:0.35, fy:0.3, stop:0 rgba(255, 214, 0, 0.5), stop:0.6 rgba(180, 150, 10, 0.9), stop:1 rgba(100, 80, 5, 0.98)); "
                                   "color: #ffd600; font-size: 12px; font-weight: bold; text-align: center; "
                                   "box-shadow: inset 0 1px 2px rgba(255,255,255,0.3), inset 0 -2px 4px rgba(0,0,0,0.4), 0 0 20px rgba(255, 214, 0, 0.45), 0 3px 10px rgba(200,150,0,0.65);";
        btnLand->setStyleSheet(landYellowStyle);
        btnLand->style()->unpolish(btnLand);
        btnLand->style()->polish(btnLand);
        btnHover->setStyleSheet(defaultStyle);
        btnHover->style()->unpolish(btnHover);
        btnHover->style()->polish(btnHover);
        btnTakeoff->setStyleSheet(defaultStyle);
        btnTakeoff->style()->unpolish(btnTakeoff);
        btnTakeoff->style()->polish(btnTakeoff);

    } else if (fm == Px4Controller::Mode_Ground_Idle) {
        // Ground_Idle 模式：无人机在地面上且未解锁（已落地或从未起飞）
        isFlying = false;       // 不在飞行
        isLanding = false;      // 不在降落
        isHovering = false;     // 不在悬停
        px4WasInAir = false;    // 重置"曾经升空"标志，为下次起飞做准备

        // 所有按钮恢复默认样式
        btnLand->setStyleSheet(defaultStyle);                      // 降落按钮恢复默认
        btnLand->style()->unpolish(btnLand);                       // 刷新样式
        btnLand->style()->polish(btnLand);
        btnHover->setStyleSheet(defaultStyle);                     // 悬停按钮恢复默认
        btnHover->style()->unpolish(btnHover);                     // 刷新样式
        btnHover->style()->polish(btnHover);
        btnTakeoff->setStyleSheet(defaultStyle);                   // 起飞按钮恢复默认
        btnTakeoff->style()->unpolish(btnTakeoff);                 // 刷新样式
        btnTakeoff->style()->polish(btnTakeoff);

        // 保持 px4PollTimer 运行（PX4 仍会发送遥测，高度等数据自然归零）
        // 不启动 dataUpdateTimer（避免与 PX4 数据冲突）
        setShortcutButtonsEnabled(true);                           // 启用快捷按钮

    } else if (fm == Px4Controller::Mode_Ground_Armed) {
        // Ground_Armed 模式：PX4 已解锁但还未起飞（解锁后到起飞指令执行之间的过渡状态）
        // 保持之前的 isFlying 状态不变：
        //   - 如果是起飞过程中经过此状态，isFlying 保持 true（避免悬停/降落按钮被禁用）
        //   - 如果是降落后经过此状态，isFlying 会被其他逻辑设为 false
        isLanding = false;    // 不在降落中
        isHovering = false;   // 不在悬停中
        // 注意：不修改 isFlying，保持之前的值
    }

    qDebug() << "[Widget] PX4 飞行模式:" << modeName;
}

/**
 * @brief PX4 遥测界面刷新定时器槽（1Hz）
 * @details 当 PX4 已连接时，刷新界面显示；未连接时由原有模拟逻辑负责。
 *          这是为了避免与 dataUpdateTimer 的模拟逻辑冲突而独立设计的。
 */
void Widget::onPx4PollTimer()
{
    if (!px4Connected || !px4Controller) return;  // PX4 未连接或控制器不存在，直接返回

    // 兜底检查：无人机曾经升空，现在高度/速度归零且未解锁，说明已落地
    // 该检查作为飞行模式回调（onPx4FlightModeChanged）的补充，
    // 确保即使 PX4 飞行模式没有正确变化，也能检测到降落完成。
    // 条件说明：
    //   px4WasInAir  - 曾经升空过（避免起飞前误判）
    //   isFlying/isHovering - 当前界面状态认为在飞行/悬停
    //   !isLanding   - 不在降落中（降落中由其他逻辑处理）
    //   flightHeight < 0.3 - 高度低于 0.3m
    //   flightSpeed < 1.0  - 速度低于 1km/h
    //   !px4Armed    - 无人机已上锁
    if (px4WasInAir && (isFlying || isHovering) && !isLanding &&
        flightHeight < 0.3 && flightSpeed < 1.0 && !px4Armed) {
        qDebug() << "[Widget] 检测到无人机已落地 (高度=" << flightHeight  // 调试日志：打印落地时的高度
                 << "m, 速度=" << flightSpeed << "km/h)";                  // 调试日志：打印落地时的速度
        isFlying = false;       // 重置飞行状态
        isHovering = false;     // 重置悬停状态
        isLanding = false;      // 重置降落状态
        px4WasInAir = false;    // 重置"曾经升空"标志，为下次起飞做准备

        // 定义默认按钮样式字符串（浅青色立体效果）
        QString defaultStyle = "width: 55px; height: 55px; border-radius: 27px; "
                               "border: 1.5px solid rgba(0, 212, 255, 0.7); border-top: 1.5px solid rgba(140, 215, 245, 0.9); border-left: 1.5px solid rgba(110, 200, 235, 0.85); border-right: 1.5px solid rgba(0, 80, 110, 0.9); border-bottom: 1.5px solid rgba(0, 55, 80, 0.95);"
                               "background: qradialgradient(cx:0.3, cy:0.25, radius:0.9, fx:0.3, fy:0.25, stop:0 rgba(82, 108, 138, 0.95), stop:0.4 rgba(42, 56, 72, 0.93), stop:0.8 rgba(20, 28, 38, 0.97), stop:1 rgba(6, 10, 16, 1)); "
                               "color: #00d4ff; font-size: 12px; font-weight: bold; text-align: center; "
                               "box-shadow: inset 0 1px 2px rgba(0, 212, 255, 0.25), inset 0 -2px 4px rgba(0,0,0,0.5), 0 0 10px rgba(0, 212, 255, 0.15), 0 3px 8px rgba(0,0,0,0.65);";
        btnLand->setStyleSheet(defaultStyle);                      // 降落按钮恢复默认
        btnLand->style()->unpolish(btnLand);                       // 刷新样式
        btnLand->style()->polish(btnLand);
        btnHover->setStyleSheet(defaultStyle);                     // 悬停按钮恢复默认
        btnHover->style()->unpolish(btnHover);                     // 刷新样式
        btnHover->style()->polish(btnHover);
        btnTakeoff->setStyleSheet(defaultStyle);                   // 起飞按钮恢复默认
        btnTakeoff->style()->unpolish(btnTakeoff);                 // 刷新样式
        btnTakeoff->style()->polish(btnTakeoff);

        setShortcutButtonsEnabled(true);                           // 启用快捷按钮
    }

    // 直接用已被 onPx4TelemetryUpdated 更新的成员变量刷新界面
    lblFlightHeight->setText("高度: " + QString::number(flightHeight, 'f', 2) + "m");           // 刷新高度显示（2位小数）
    lblFlightSpeed->setText("速度: " + QString::number(flightSpeed, 'f', 2) + "km/h");         // 刷新速度显示（2位小数）
    lblCoordinates->setText("纬度: " + QString::number(latitude, 'f', 6) + "°N\n"              // 刷新纬度显示（6位小数）
                           + "经度: " + QString::number(longitude, 'f', 6) + "°E");            // 刷新经度显示（6位小数）
    lblBatteryLevel->setText("电量");
    // PX4 模式下直接用单数值电池电量刷新显示
    for (int i = 0; i < 6; i++) {
        if (batteryBars[i]) {
            if (i == 0) {
                // 第一节电池显示 PX4 真实电量
                int pct = batteryLevel;
                QString color;
                if (pct >= 60) color = "#88ffcc";
                else if (pct >= 30) color = "#ffff88";
                else color = "#ff8888";
                batteryBars[i]->setStyleSheet(
                    QString("background: %1; border-radius: 2px;").arg(color));
            } else {
                batteryBars[i]->setStyleSheet("background: #444; border-radius: 2px;");
            }
        }
    }
    if (lblBatteryPercent) {
        lblBatteryPercent->setText(QString::number(batteryLevel) + "%");
    }
    if (lblBatteryIndex) {
        lblBatteryIndex->setText("PX4 电池");
    }

    lblDistance->setText("目的地距离：" + QString::number(destinationDistance, 'f', 2) + "km");
    lblTotalDistance->setText("总行驶距离：" + QString::number(totalDistance, 'f', 2) + "km");
    lblTraveledDistance->setText("已行驶距离：" + QString::number(traveledDistance, 'f', 2) + "km");

    // PX4 状态标签追加模式信息
    if (lblPx4Status) {
        Px4Controller::FlightMode fm = static_cast<Px4Controller::FlightMode>(px4FlightMode);
        QString modeShort;
        switch (fm) {
        case Px4Controller::Mode_Ground_Idle:    modeShort = "待机"; break;
        case Px4Controller::Mode_Ground_Armed:   modeShort = "待飞"; break;
        case Px4Controller::Mode_Takeoff:        modeShort = "起飞"; break;
        case Px4Controller::Mode_Hold:           modeShort = "悬停"; break;
        case Px4Controller::Mode_Mission:        modeShort = "任务"; break;
        case Px4Controller::Mode_Return:         modeShort = "返航"; break;
        case Px4Controller::Mode_Land:           modeShort = "降落"; break;
        case Px4Controller::Mode_Offboard:       modeShort = "OFFB"; break;
        case Px4Controller::Mode_Manual:         modeShort = "手动"; break;
        default:                                 modeShort = "?"; break;
        }
        lblPx4Status->setText(QString("PX4: 已连接 [%1] %2%")
                                  .arg(modeShort)
                                  .arg(batteryLevel));
    }

    updateShortcutButtonHighlight();  // 根据当前高度和速度数据更新快捷按钮高亮状态
}

