/**
 * @file widget.h
 * @brief 飞行数据界面主窗口类头文件
 * @details 该类实现了一个基于Qt Widgets的飞行数据显示界面，采用纯代码方式创建UI布局，
 *          不依赖Qt Designer生成的.ui文件。界面风格为渐变浅青光色科技风格。
 */

#ifndef WIDGET_H       // 头文件保护宏，防止头文件被重复包含
#define WIDGET_H       // 定义头文件保护宏

#include <QWidget>          // 包含QWidget类头文件，作为Widget类的基类
#include <QPushButton>      // 包含QPushButton类头文件，用于创建按钮控件
#include <QLabel>           // 包含QLabel类头文件，用于创建标签控件
#include <QLineEdit>        // 包含QLineEdit类头文件，用于创建输入框控件
#include <QGridLayout>      // 包含QGridLayout类头文件，用于网格布局（预留）
#include "compasswidget.h"   // 包含指南针控件类头文件
#include <QVBoxLayout>      // 包含QVBoxLayout类头文件，用于垂直布局
#include <QHBoxLayout>      // 包含QHBoxLayout类头文件，用于水平布局
#include <QFrame>           // 包含QFrame类头文件，用于创建框架容器
#include <QProgressBar>     // 包含QProgressBar类头文件（预留）
#include <QButtonGroup>     // 包含QButtonGroup类头文件，用于按钮互斥选择
#include <QMessageBox>      // 包含QMessageBox类头文件，用于弹出提示窗口
#include <QDialog>          // 包含QDialog类头文件，用于创建对话框
#include <QTimer>           // 包含QTimer类头文件，用于定时更新数据
#include <QDateTime>        // 包含QDateTime类头文件，用于日期时间处理
#include <QTimeZone>        // 包含QTimeZone类头文件，用于指定时区获取当前时间
#include <QImage>           // 包含QImage类头文件，用于地图图片加载
#include <QPixmap>          // 包含QPixmap类头文件，用于地图图片显示
#include "mapdialog.h"      // 包含地图弹窗类头文件
#include "minimapdialog.h"  // 包含小地图弹窗类头文件
#include "rotationdial.h"   // 包含旋转圆盘控件类头文件
#include "px4controller.h"  // 包含 PX4 控制器类头文件
#include "radarwidget.h"     // 包含雷达扫描控件类头文件
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QCryptographicHash>
#include <QProcess>
#include <QWheelEvent>
#include <QGeoPositionInfoSource>
#include <QGeoCoordinate>
#include <QGraphicsDropShadowEffect>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QMoveEvent>
#include <QWebEngineView>       // 包含QWebEngineView类头文件，用于位置信息框动态地图
#include <QWebEnginePage>       // 包含QWebEnginePage类头文件，用于位置信息框动态地图页面
#include <QWebEngineSettings>   // 包含QWebEngineSettings类头文件，用于配置WebEngine设置

// OpenCV 前置声明（实际头文件在 widget.cpp 中包含，避免污染头文件编译单元）
namespace cv { class VideoCapture; }


/**
 * @class Widget
 * @brief 飞行数据界面主窗口类
 * @details 该类继承自QWidget，通过纯代码方式构建飞行数据显示界面，包含以下功能区域：
 *          - 顶部标题栏（显示标题和编号）
 *          - 第一行控制按钮区（电源、起飞、指挥中心、降落、能源更换）
 *          - 第二行（指南针、目的地输入、时间显示）
 *          - 第三行（飞行高度、飞行速度、灯光控制）
 *          - 第四行（经纬度、电池电量、鸣笛控制）
 *          - 第五行（目的地距离、当前位置）
 */
class Widget : public QWidget  // 定义Widget类，继承自QWidget基类
{
    Q_OBJECT  // Qt元对象系统宏，使类支持信号槽机制、属性系统等

public:
    /**
     * @brief 构造函数
     * @param parent 父窗口指针，默认为nullptr
     */
    Widget(QWidget *parent = nullptr);  // 声明构造函数，接收父窗口指针参数，默认值为nullptr
    
    /**
     * @brief 析构函数
     */
    ~Widget();  // 声明析构函数

private:
    /**
     * @brief 初始化UI界面布局
     * @details 创建所有控件并设置布局结构，不包含样式设置
     */
    void setupUI();  // 声明setupUI()私有方法，用于初始化UI布局
    
    /**
     * @brief 应用界面样式
     * @details 设置渐变浅青光色科技风格的QSS样式表
     */
    void applyStyles();  // 声明applyStyles()私有方法，用于应用界面样式
    
    /**
     * @brief 灯光按钮点击事件处理
     * @details 实现灯光状态切换：灯光→近光→远光→灯光（关闭提示）
     */
    void onLightsClicked();  // 声明灯光按钮点击处理槽函数
    void onLowLightClicked();  // 声明近光按钮点击处理槽函数
    void onHighLightClicked();  // 声明远光按钮点击处理槽函数
    
    /**
     * @brief 鸣笛按钮按压事件处理
     * @details 实现鸣笛按压逻辑：按压时按钮变橙色
     */
    void onHornPressed();  // 声明鸣笛按钮按压处理槽函数
    
    /**
     * @brief 鸣笛按钮释放事件处理
     * @details 实现鸣笛释放逻辑：释放后按钮恢复原始样式
     */
    void onHornReleased();  // 声明鸣笛按钮释放处理槽函数
    
    /**
     * @brief 指南针按钮点击事件处理
     * @details 弹出指南针窗口，显示大号指南针
     */
    void onCompassClicked();  // 声明指南针按钮点击处理槽函数
    
    /**
     * @brief 地图按钮点击事件处理
     * @details 弹出地图窗口，显示当前位置和周围地图
     */
    void onMapClicked();  // 声明地图按钮点击处理槽函数
    
    /**
     * @brief 电源按钮点击事件处理
     * @details 实现电源开关逻辑：第一次点击开机刷新界面，第二次点击关机
     */
    void onPowerClicked();  // 声明电源按钮点击处理槽函数
    
    /**
     * @brief 起飞按钮点击事件处理
     * @details 实现起飞逻辑：点击后按钮变蓝色，启动数据更新定时器
     */
    void onTakeoffClicked();  // 声明起飞按钮点击处理槽函数
    
    /**
     * @brief 悬停按钮点击事件处理
     * @details 实现悬停逻辑：点击后按钮变红色
     */
    void onHoverClicked();  // 声明悬停按钮点击处理槽函数
    
    /**
     * @brief 降落按钮点击事件处理
     * @details 实现降落逻辑：点击后开始降落，停稳后数据静止
     */
    void onLandClicked();  // 声明降落按钮点击处理槽函数
    
    /**
     * @brief 指挥中心按钮点击事件处理
     * @details 实现指挥中心逻辑：点击后按钮变绿色
     */
    void onCommandCenterClicked();  // 声明指挥中心按钮点击处理槽函数
    
    /**
     * @brief 归还按钮点击事件处理
     * @details 实现归还逻辑：点击后按钮变紫红色，5秒后恢复原始状态
     */
    void onReturnClicked();  // 声明归还按钮点击处理槽函数
    
    /**
     * @brief 能源更换按钮点击事件处理
     * @details 实现能源更换逻辑：点击后按钮变黄色
     */
    void onEnergyReplaceClicked();  // 声明能源更换按钮点击处理槽函数
    
    /**
     * @brief 更换航线按钮点击事件处理
     * @details 实现更换航线逻辑：点击后按钮变绿色
     */
    void onRouteReplaceClicked();  // 声明更换航线按钮点击处理槽函数

    /**
     * @brief 启动视频显示（连接摄像头并显示画面）
     * @details 点击"降落"时调用：枚举摄像头索引 0~9，逐个尝试打开并读取测试帧，
     *          选中第一个能真正采集画面的设备（含笔记本自带摄像头、USB 摄像头）。
     *          成功后启动帧采集定时器（约 30 FPS），将画面显示到视频显示窗口；
     *          全部不可用时在窗口显示"摄像头未连接"提示。视频仅在点击降落时启动。
     */
    void startVideoDisplay();  // 声明启动视频显示方法

    /**
     * @brief 停止视频显示（断开摄像头并恢复占位）
     * @details 关机或析构时调用：停止采集定时器、释放摄像头资源，
     *          并将视频显示窗口恢复为"视频显示窗口"占位文字。
     */
    void stopVideoDisplay();  // 声明停止视频显示方法

    /**
     * @brief 视频帧采集定时器超时槽函数
     * @details 从 OpenCV VideoCapture 读取一帧（BGR），转为 RGB 后构造 QImage，
     *          按视频显示窗口尺寸等比缩放并显示。空帧或摄像头未就绪时跳过。
     */
    void onVideoFrameReady();  // 声明视频帧采集超时槽函数
    
    /**
     * @brief 更新飞行数据
     * @details 定时器触发的槽函数，用于实时更新飞行数据
     */
    void updateFlightData();  // 声明数据更新槽函数
    
    /**
     * @brief 更新6节电池的显示
     * @details 根据每节电池的独立电量和当前使用的电池编号，
     *          更新电池能量条颜色、总电量百分比、当前电池编号显示。
     *          颜色规则：未安装或已耗尽显示灰色，当前使用显示蓝色，高电量绿色，中电量黄色，低电量红色
     */
    void updateBatteryDisplay();
    
    /**
     * @brief 获取总电量百分比
     * @return 6节电池的平均总电量百分比（所有电池电量之和 / 6）
     * @details 未安装的电池电量按0计算，因此存在未安装电池时总电量不会达到100%
     */
    int getTotalBatteryLevel() const;
    
    /**
     * @brief 消耗当前电池电量
     * @param amount 消耗的电量百分比（通常为1）
     * @details 从当前正在使用的电池中扣除指定电量：
     *          1. 若当前电池未安装则跳转到下一节已安装电池
     *          2. 电池耗尽时自动切换到下一节已安装且有电的电池
     *          3. 所有电池耗尽后自动重新充满所有已安装电池
     *          4. 只剩最后一节已安装电池且电量<=20%时弹窗警告
     */
    void consumeBattery(int amount);
    
    /**
     * @brief 刷新界面数据
     * @details 将界面所有数据设置为初始值
     */
    void refreshUI();  // 声明刷新界面函数
    
    /**
     * @brief 重置按钮样式
     * @details 将所有按钮恢复到默认样式
     */
    void resetButtonStyles();  // 声明重置按钮样式函数
    
    /**
     * @brief 更新时间显示
     * @details 更新当前时间和日期显示
     */
    void updateTime();  // 声明更新时间函数
    
    /**
     * @brief 更新飞行时长显示
     * @details 每秒更新飞行时长，格式为HH:MM:SS
     */
    void updateFlightDuration();  // 声明更新飞行时长函数
    
    /**
     * @brief 更新地图位置显示
     * @details 将当前经纬度数据实时更新到地图弹窗
     */
    void updateMapPosition();  // 声明更新地图位置函数
    
    /**
     * @brief 获取精确位置（跨平台实现）
     * @details Windows平台：使用QProcess调用PowerShell脚本，通过WinRT Geolocation API获取精确经纬度
     *          Linux平台：直接使用高德IP定位API获取位置
     *          定位失败时回退到高德IP定位
     */
    void requestRealLocation();  // 声明获取真实位置函数
    void loadMapToPositionFrame(double lon, double lat);  // 声明加载地图到位置框函数（动态API地图）
    void initPositionMap();                               // 声明初始化位置信息框动态地图函数
    void updatePositionWebView(double lon, double lat, bool forceCenter = false);  // 声明更新位置信息框动态地图函数
    void checkPositionMapReady();                         // 声明检查位置信息框地图就绪函数
    void setPositionStatus(const QString &msg);           // 声明设置位置信息框状态文字函数
    void syncPositionMapGeometry();                      // 声明同步位置信息框地图子窗口几何位置函数
    
    /**
     * @brief 处理GPS定位脚本输出（仅Windows平台使用）
     * @details 解析PowerShell脚本输出的经纬度数据，更新界面和地图
     *          如果GPS定位失败，回退到高德IP定位
     *          Linux平台直接使用IP定位，不调用此函数
     */
    void onGpsLocationReceived();  // 声明GPS定位结果处理函数
    
    /**
     * @brief 启动Linux GPS定位
     * @details 使用Qt Positioning模块获取GPS定位，失败时回退到IP定位
     */
    void startLinuxGps();
    
    /**
     * @brief 处理Linux GPS位置更新
     */
    void onLinuxGpsPositionUpdated(const QGeoPositionInfo &info);
    
    /**
     * @brief 处理Linux GPS定位错误
     */
    void onLinuxGpsError(QGeoPositionInfoSource::Error error);
    
    /**
     * @brief 通过高德IP定位API获取位置（回退方案）
     * @details 当GPS定位失败时，使用高德地图IP定位接口获取城市级别位置
     */
    void requestIPLocation();  // 声明IP定位函数
    
    /**
     * @brief 处理IP定位响应
     * @details 解析高德IP定位API返回的JSON数据，提取经纬度
     */
    void onIpLocationResponse(QNetworkReply *reply);  // 声明IP定位响应处理函数
    
    /**
     * @brief 设置高度和速度快捷按钮的可用性
     * @details 根据电源状态和飞行状态控制按钮是否可点击
     * @param enabled 是否启用按钮
     */
    void setShortcutButtonsEnabled(bool enabled);  // 声明设置快捷按钮可用性函数

    /**
     * @brief 更新快捷按钮高亮状态
     * @details 此函数已改为空操作，快捷按钮的米色高亮状态完全由用户点击控制
     *          点击后通过 setStyleSheet 内联样式设置米色背景并保持
     *          点击同组其他按钮时上一个按钮自动恢复默认样式
     */
    void updateShortcutButtonHighlight();  // 声明更新快捷按钮高亮状态函数

    /**
     * @brief 清除所有快捷按钮的高亮状态
     * @details 清除所有高度和速度快捷按钮的内联样式，使其恢复QSS默认外观
     *          主要用于结束飞行后重置快捷按钮的视觉状态
     */
    void clearShortcutButtonHighlight();  // 声明清除快捷按钮高亮状态函数

    /**
     * @brief 将指定快捷按钮设置为米色高亮状态
     * @details 参考起飞/悬停/降落按钮的实现方式，通过 setStyleSheet 内联样式
     *          设置米色背景。调用前会先清除同组其他按钮的内联样式，确保互斥效果
     * @param btn 需要设置为米色的按钮指针
     * @param group 该按钮所属的互斥分组（高度组或速度组）
     */
    void setShortcutButtonBeige(QPushButton* btn, QButtonGroup* group);  // 声明设置快捷按钮米色样式函数
    
    /**
     * @brief 获取天干地支年份
     * @details 根据公历年份计算对应的天干地支年份
     */
    QString getGanZhiYear(int year);  // 声明获取天干地支年份函数
    
    /**
     * @brief 目的地输入框回车事件处理
     * @details 获取输入的目的地地址，通过百度地图API获取坐标并计算距离
     */
    void onDestinationEntered();  // 声明目的地输入处理槽函数
    void onDestinationTextChanged(const QString &text);  // 声明目的地文本变化处理槽函数
    void onDestSearchTimeout();  // 声明目的地搜索防抖定时器触发槽函数
    void showMiniMap();  // 声明显示小地图窗口函数
    void hideMiniMap();  // 声明隐藏小地图窗口函数
    void mousePressEvent(QMouseEvent *event) override;  // 重写鼠标按下事件
    void mouseMoveEvent(QMouseEvent *event) override;  // 重写鼠标移动事件，用于地图拖动
    void mouseReleaseEvent(QMouseEvent *event) override;  // 重写鼠标释放事件，用于结束地图拖动
    void mouseDoubleClickEvent(QMouseEvent *event) override;  // 重写鼠标双击事件，用于地图还原
    void wheelEvent(QWheelEvent *event) override;       // 重写鼠标滚轮事件，用于地图缩放
    
    /**
     * @brief 处理目的地地理编码响应
     * @details 解析百度地图地理编码API返回的JSON数据
     */
    void onGeocodeResponse(QNetworkReply *reply);  // 声明地理编码响应处理槽函数
    
    /**
     * @brief 计算两点之间的距离（Haversine公式）
     * @details 根据经纬度计算两个位置之间的距离
     * @param lat1 起点纬度
     * @param lon1 起点经度
     * @param lat2 终点纬度
     * @param lon2 终点经度
     * @return 距离（公里）
     */
    double calculateDistance(double lat1, double lon1, double lat2, double lon2);  // 声明距离计算函数
    
    /**
     * @brief 计算高德地图API签名（HMAC-SHA256）
     * @param url 完整URL（不含sig参数）
     * @param securityKey 安全密钥
     * @return 签名字符串
     */
    QString calculateSignature(const QString &url, const QString &securityKey);  // 声明签名计算函数
    
    /**
     * @brief HMAC-SHA256计算
     * @param data 待签名数据
     * @param key 密钥
     * @return HMAC-SHA256结果
     */
    QByteArray hmacSha256(const QByteArray &data, const QByteArray &key);  // 声明HMAC-SHA256计算函数
    
    /**
     * @brief 事件过滤器
     * @details 用于捕获目的地输入框的焦点事件
     */
    bool eventFilter(QObject *obj, QEvent *event) override;  // 声明事件过滤器

    /**
     * @brief 显示事件（重写基类方法）
     * @details 窗口显示后微调指南针位置（左移20px），绕过QBoxLayout对setFixedSize控件的margin限制
     */
    void showEvent(QShowEvent *event) override;

    /**
     * @brief 隐藏事件（重写基类方法）
     * @details 主窗口被隐藏/最小化时，同步隐藏 positionWebView 独立顶层窗口
     */
    void hideEvent(QHideEvent *event) override;

    /**
     * @brief 移动事件（重写基类方法）
     * @details 主窗口移动时同步位置信息框地图顶层窗口的位置，
     *          确保 ToolTip 窗口始终覆盖在占位控件上方
     */
    void moveEvent(QMoveEvent *event) override;

    /**
     * @brief 尺寸变化事件（重写基类方法）
     * @details 同步位置信息框动态地图的位置和大小，
     *          确保与占位控件精确对齐，视觉上实现嵌入效果
     */
    void resizeEvent(QResizeEvent *event) override;

    /**
     * @brief 窗口状态变化事件（重写基类方法）
     * @details 主窗口最小化/恢复时同步 positionWebView 的可见性，
     *          确保独立顶层 ToolTip 窗口跟随主窗口状态
     */
    void changeEvent(QEvent *event) override;

    /**
     * @brief 绘制事件（重写基类方法）
     * @details 在主界面背景上绘制科技感网格纹理和扫描线动画
     */
    void paintEvent(QPaintEvent *event) override;

    /**
     * @brief 为关键面板和控件设置霓虹发光效果
     * @details 使用QGraphicsDropShadowEffect为信息面板、标题栏等添加青蓝霓虹外发光
     */
    void setupNeonGlowEffects();

    /**
     * @brief 扫描线动画定时器槽函数
     * @details 每次触发时更新扫描线位置并触发重绘
     */
    void onScanLineTimer();
    
    /**
     * @brief 生成科技感图标
     * @details 使用QPixmap绘制科技风格的图标，与界面背景配色匹配
     * @param type 图标类型：0=高度, 1=速度, 2=时长, 3=坐标, 4=距离, 5=重量, 6=电池, 7=地图
     * @return QPixmap图标
     */
    QPixmap createTechIcon(int type);  // 声明生成科技感图标函数
    
    /**
     * @brief 灯光状态枚举
     * @details 0=关闭(灯光), 1=近光, 2=远光
     */
    int lightsState;  // 灯光状态变量
    
    /**
     * @brief 灯光样式表字符串缓存
     */
    QString lightsOffStyle;   // 灯光关闭样式
    QString lightsOnStyle;    // 灯光开启样式（无近远光）
    QString lightsLowStyle;   // 近光样式（浅绿色）
    QString lightsHighStyle;  // 远光样式（纯绿色）
    QString lowLightOffStyle; // 近光按钮关闭样式
    QString lowLightOnStyle;  // 近光按钮开启样式
    QString highLightOffStyle; // 远光按钮关闭样式
    QString highLightOnStyle;  // 远光按钮开启样式
    
    // 控制按钮成员变量（圆形按钮）
    QPushButton *btnPower;              ///< 电源按钮（圆形），用于控制设备电源开关
    QPushButton *btnTakeoff;            ///< 起飞按钮（圆形），用于控制起飞操作
    QPushButton *btnHover;              ///< 悬停按钮（圆形），用于控制悬停操作
    QPushButton *btnLand;               ///< 降落按钮（圆形），用于控制降落操作
    QPushButton *btnCommandCenter;      ///< 指挥中心按钮（圆形），用于连接指挥中心
    QPushButton *btnReturn;             ///< 归还按钮（圆形），用于归还设备
    QTimer *returnResetTimer;           ///< 归还按钮状态重置定时器，5秒后恢复
    CompassWidget *compassWidget;       ///< 指南针控件（直接显示样式，不可点击）
    MapDialog *mapDialog;               ///< 地图弹窗对话框
    MiniMapDialog *miniMapDialog;       ///< 目的地输入小地图弹窗
    QPushButton *btnLights;             ///< 灯光控制按钮（圆形），用于控制设备灯光
    QPushButton *btnLowLight;           ///< 近光按钮（小椭圆形），用于控制近光灯
    QPushButton *btnHighLight;          ///< 远光按钮（小椭圆形），用于控制远光灯
    QPushButton *btnHorn;               ///< 鸣笛按钮（圆形），用于控制鸣笛功能
    QPushButton *btnEnergyReplace;      ///< 能源更换按钮（矩形），用于触发能源更换流程
    QPushButton *btnRouteReplace;       ///< 更换航线按钮（椭圆），用于切换飞行航线
    
    // 高度快捷按钮
    QPushButton *btn1m5;               ///< 1.5米高度快捷按钮
    QPushButton *btn2m;                ///< 2.0米高度快捷按钮
    QPushButton *btn3m;                ///< 3米高度快捷按钮
    QPushButton *btn0m6;               ///< 0.6米高度快捷按钮
    QPushButton *btn1m0;               ///< 1.0米高度快捷按钮
    
    // 速度快捷按钮
    QPushButton *btn3km;               ///< 3 km/h速度快捷按钮（速度标签右边）
    QPushButton *btn5kmLow;            ///< 5 km/h速度快捷按钮（速度标签右边）
    QPushButton *btn10m;               ///< 10 km/h速度快捷按钮
    QPushButton *btn25km;              ///< 15 km/h速度快捷按钮
    QPushButton *btn1km;               ///< 25 km/h速度快捷按钮
    QPushButton *btn5km;               ///< 30 km/h速度快捷按钮

    // 快捷按钮互斥分组（实现同一组内只能有一个按钮被选中）
    QButtonGroup *heightBtnGroup;      ///< 高度快捷按钮互斥组，确保同一时间只有一个高度按钮高亮
    QButtonGroup *speedBtnGroup;       ///< 速度快捷按钮互斥组，确保同一时间只有一个速度按钮高亮
    // 飞行状态变量
    bool isPowerOn;                     ///< 电源状态：true=开机，false=关机
    bool isFlying;                      ///< 飞行状态：true=飞行中，false=未起飞/已降落
    bool isHovering;                    ///< 悬停状态：true=悬停中，false=未悬停
    bool isLanding;                     ///< 降落状态：true=正在降落，false=未降落/已停稳

    /**
     * @brief 悬停模式枚举
     * @details 定义飞行器在空中悬停时的三种运动方式：
     *          - CIRCLE_HOVER: 盘旋悬停，飞行器在原地缓慢360°转圈（每秒旋转10°）
     *          - FORWARD_HOVER: 前进悬停，飞行器以1km/h速度缓慢向前移动
     *          - BACKWARD_HOVER: 后退悬停，飞行器以1km/h速度缓慢后退，最多持续60秒后自动切换为盘旋
     */
    enum HoverMode {
        CIRCLE_HOVER,    ///< 盘旋悬停：飞行器在空中缓慢360°转圈
        FORWARD_HOVER,   ///< 前进悬停：飞行器以1km/h速度缓慢前进
        BACKWARD_HOVER   ///< 后退悬停：飞行器以1km/h速度缓慢后退，最多60秒
    };
    HoverMode hoverMode;                ///< 当前悬停模式，默认为 FORWARD_HOVER（前进悬停）
    int hoverBackwardSeconds;           ///< 后退悬停已持续的秒数，用于60秒超时检测
    double hoverHeading;                ///< 盘旋悬停时的航向角（0-360度），每秒递增10°

    // 数据更新定时器
    QTimer *dataUpdateTimer;            ///< 数据更新定时器，用于实时更新飞行数据
    QTimer *timeUpdateTimer;            ///< 时间更新定时器，用于实时更新时间显示
    QTimer *flightDurationTimer;        ///< 飞行时长定时器，用于更新飞行时长
    QTimer *mapUpdateTimer;             ///< 地图位置更新定时器，用于实时更新地图显示
    QTimer *destSearchTimer;           ///< 目的地搜索防抖定时器，避免频繁网络请求

    // 扫描线动画
    QTimer *m_scanLineTimer;          ///< 扫描线动画定时器（30fps）
    int m_scanLineY;                   ///< 扫描线当前Y坐标位置
    bool m_scanLineDirDown;            ///< 扫描线移动方向（true=向下，false=向上）
    
    // 网络请求相关
    QNetworkAccessManager *networkManager;  ///< 网络访问管理器，用于获取位置信息
    QProcess *gpsProcess;                   ///< GPS定位进程（仅Windows平台使用），调用PowerShell脚本获取精确位置
    QGeoPositionInfoSource *positionSource; ///< Linux GPS定位源
    bool gpsActive;                        ///< GPS是否活跃
    bool usingGps;                         ///< 当前是否使用GPS（否则为IP定位）
    
    
    
    // 飞行数据成员变量（用于实时更新）
    double flightHeight;                ///< 当前飞行高度（米）
    double targetFlightHeight;          ///< 目标飞行高度（米），飞行时固定在此高度
    double flightSpeed;                 ///< 当前飞行速度（km/h）
    double maxFlightSpeed;              ///< 当前最大速度（km/h）
    int batteryLevel;                   ///< 当前电池电量（%）
    double flightWeight;                ///< 当前重量（kg）
    double latitude;                    ///< 当前纬度
    double longitude;                   ///< 当前经度
    double takeoffLatitude;             ///< 起飞点纬度
    double takeoffLongitude;            ///< 起飞点经度
    int flightSeconds;                  ///< 当前飞行时长（秒）
    int totalFlightSeconds;             ///< 总飞行时长（秒），出厂以来累计，持久化保存
    double destinationDistance;         ///< 目的地距离（公里）
    double totalDistance;               ///< 总距离（公里），从起飞点到目的地的总距离
    double traveledDistance;            ///< 已行驶距离（公里），从起飞点到当前位置的距离
    double destLongitude;               ///< 目的地经度
    double destLatitude;                ///< 目的地纬度
    QString currentCity;                ///< 当前所在城市，用于地理编码限定搜索范围
    
    // 输入控件成员变量
    QLineEdit *leDestination;           ///< 目的地输入框，用于输入目标位置
    
    // 数据显示标签成员变量
    QLabel *lblFlightHeight;            ///< 飞行高度显示标签，显示当前飞行高度数值
    QLabel *lblFlightSpeed;             ///< 飞行速度显示标签，显示当前飞行速度数值
    QLabel *lblCoordinates;             ///< 经纬度显示标签，显示当前位置经纬度信息
    QLabel *lblBatteryLevel;            ///< 电池电量显示标签，显示电池电量状态
    QLabel *lblBatteryPercent;          ///< 电池电量百分比标签，显示电量百分比
    QLabel *lblFlyableDistance;         ///< 当前电量可飞行距离标签，显示剩余电量还能飞行的距离
    QProgressBar *batteryBar;           ///< 电池能量条，显示总电量进度
    QFrame *batteryBars[6];             ///< 6个独立电池电量条（从左到右依次消耗），根据电量显示不同颜色
    QLabel *lblBatteryIndex;           ///< 当前使用的电池编号标签，显示"电池 X/Y"格式
    int batteryPercentages[6];          ///< 6节电池的独立电量（%），每节电池独立监控
    bool batteryInstalled[6];           ///< 6节电池是否已安装（false=未安装，显示灰色）
    int currentBatteryIndex;            ///< 当前正在使用的电池编号（0-5），前一个电池耗尽后自动切换到下一个
    bool lowBatteryWarned;              ///< 最后一节已安装电池低电量弹窗是否已显示，避免重复弹窗
    QLabel *lblDistance;                ///< 目的地距离显示标签，显示到目的地的距离
    QLabel *lblTotalDistance;           ///< 总距离显示标签，显示从起飞点到目的地的总距离
    QLabel *lblTraveledDistance;        ///< 已行驶距离显示标签，显示从起飞点到当前位置的距离
    RadarWidget *radarWidget;           ///< 雷达扫描控件，显示动态雷达扫描图形
    QLabel *lblBeidouCount;             ///< 北斗卫星数量显示标签
    QLabel *lblGPSCount;                ///< GPS卫星数量显示标签
    QLabel *lblGLONASSCount;            ///< GLONASS卫星数量显示标签
    QLabel *lblGalileoCount;            ///< Galileo卫星数量显示标签
    QWebEngineView *positionWebView;    ///< 顶层ToolTip窗口动态地图视图（Qt::ToolTip + FramelessWindowHint）
    QWebEnginePage *positionWebPage;    ///< 位置信息框动态地图页面
    QWidget *positionMapPlaceholder;    ///< 布局占位控件（占据地图显示区域大小，用于几何定位参考）
    QFrame *positionFramePtr;           ///< 位置信息框框架指针（用于几何位置计算）
    QLabel *mapIconLabel;               ///< 地图框图标标签（通过代码自由定位，不参与布局）
    QLabel *mapTitleLabel;              ///< 地图框标题标签（通过代码自由定位，不参与布局）
    QLabel *videoDisplayLabel;          ///< 视频显示窗口标签（用于显示视频帧画面）
    cv::VideoCapture *videoCapture;     ///< OpenCV 视频采集对象（USB 摄像头，降落时启动）
    QTimer *videoTimer;                 ///< 视频帧采集定时器（约 30 FPS，驱动画面刷新）
    bool positionMapReady;              ///< 位置信息框动态地图是否就绪
    bool positionMapLoaded;             ///< 位置信息框动态地图页面是否已加载（延迟到首次显示后加载）
    int positionRetryCount;             ///< 位置信息框地图就绪检查重试计数
    double lastPosSentLon;              ///< 上次发送到位置信息框地图的经度（去重）
    double lastPosSentLat;              ///< 上次发送到位置信息框地图的纬度（去重）
    QString positionStatus;             ///< 位置信息框当前状态文字（用于判断定位是否进行中）
    QLabel *lblTime;                    ///< 时间显示标签，显示当前日期和时间
    QLabel *lblSerialNumber;            ///< 编号显示标签，显示设备唯一编号
    QLabel *lblWeight;                  ///< 重量显示标签，显示当前重量信息
    QLabel *lblWeightValue;             ///< 重量数值标签，显示限重状态
    QLabel *lblFlightDuration;          ///< 飞行时长显示标签，显示总飞行时长标题
    QLabel *lblDurationValue;           ///< 总飞行时长数值标签，显示出厂以来总时长
    QLabel *lblCurrentDurationValue;    ///< 当前飞行时长数值标签，显示本次起飞时长

    bool m_positionMapWasVisible;       ///< 地图弹窗打开前位置地图是否可见（用于关闭后恢复）

    // 悬停模式选择对话框（非模态，点击立即生效）
    QDialog *hoverModeDialog;           ///< 悬停模式选择对话框指针（非模态）
    QPushButton *btnHoverCircle;        ///< 对话框内盘旋按钮
    QPushButton *btnHoverStop;          ///< 对话框内悬停（原地不动）按钮
    QPushButton *btnHoverBack;          ///< 对话框内后退按钮

    // 盘旋旋转选择圆盘
    RotationDial *rotationDial;         ///< 旋转角度选择圆盘控件
    QDialog *rotationDialDialog;        ///< 圆盘容器对话框
    int rotationTargetAngle;            ///< 目标旋转角度（90/180/270/360）
    int rotationCurrentAngle;          ///< 当前已旋转的角度
    bool isRotating;                    ///< 是否正在旋转

    /**
     * @brief 显示旋转圆盘选择对话框
     */
    void showRotationDial();

    /**
     * @brief 隐藏旋转圆盘选择对话框
     */
    void hideRotationDial();

    /**
     * @brief 开始旋转
     * @param angle 目标旋转角度（90/180/270/360）
     */
    void startRotation(int angle);

    // ==================== PX4 仿真集成 ====================
    /**
     * @brief 初始化 PX4 控制器（构造函数中调用）
     * @details 创建 Px4Controller 实例并连接信号槽，不自动连接 PX4
     */
    void initPx4Controller();

    /**
     * @brief 处理 PX4 连接状态变化
     * @param state Px4Controller::ConnectionState 枚举值
     */
    void onPx4ConnectionStateChanged(int state);

    /**
     * @brief 处理 PX4 遥测数据更新
     * @param t 遥测数据快照
     * @details 用 PX4 真实遥测数据刷新界面（高度/速度/坐标/电池/航向），
     *          仅在 PX4 已连接且有效时覆盖本地模拟数据
     */
    void onPx4TelemetryUpdated(const Px4Controller::TelemetrySnapshot &t);

    /**
     * @brief 处理 PX4 操作结果反馈
     * @param operation 操作名（arm/takeoff/land 等）
     * @param ok 是否成功
     * @param msg 描述信息
     */
    void onPx4OperationResult(const QString &operation, bool ok, const QString &msg);

    /**
     * @brief 处理 PX4 飞行模式变化
     * @param mode Px4Controller::FlightMode 枚举值
     */
    void onPx4FlightModeChanged(int mode);

    /**
     * @brief PX4 遥测刷新定时器槽（独立于 dataUpdateTimer）
     * @details 当 PX4 已连接时，定时从 Px4Controller 拉取最新遥测并更新界面；
     *          未连接时由原有模拟逻辑负责更新
     */
    void onPx4PollTimer();

    Px4Controller *px4Controller;        ///< PX4 仿真控制器实例
    bool px4Connected;                   ///< PX4 是否已连接
    bool px4Armed;                       ///< PX4 是否已解锁（用于状态显示）
    bool px4WasInAir;                    ///< PX4 无人机是否曾经升空（高度>1m时置true，用于降落检测避免起飞前误判）
    int  px4FlightMode;                  ///< PX4 当前飞行模式
    QTimer *px4PollTimer;                ///< PX4 遥测界面刷新定时器（1Hz）
    QLabel *lblPx4Status;                ///< PX4 连接状态显示标签（左下角）
};
#endif // WIDGET_H  // 结束头文件保护宏