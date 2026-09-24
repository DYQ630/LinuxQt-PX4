/**
 * @file minimapdialog.h
 * @brief 小地图弹窗类头文件
 * @details 该类实现了一个小型地图弹窗，用于在目的地输入框附近显示位置地图，
 *          支持地图缩放、拖动和双击还原功能。使用高德地图瓦片API加载地图。
 */

#ifndef MINIMAPDIALOG_H       // 头文件保护宏，防止头文件被重复包含
#define MINIMAPDIALOG_H       // 定义头文件保护宏

#include <QDialog>              // 包含QDialog类头文件，作为MiniMapDialog类的基类
#include <QLabel>               // 包含QLabel类头文件，用于显示地图图片和坐标信息
#include <QVBoxLayout>          // 包含QVBoxLayout类头文件，用于垂直布局
#include <QNetworkAccessManager> // 包含QNetworkAccessManager类头文件，用于网络请求
#include <QNetworkRequest>      // 包含QNetworkRequest类头文件，用于创建网络请求
#include <QNetworkReply>        // 包含QNetworkReply类头文件，用于处理网络响应
#include <QImage>               // 包含QImage类头文件，用于地图图片加载和处理
#include <QPixmap>              // 包含QPixmap类头文件，用于地图图片显示
#include <QUrl>                 // 包含QUrl类头文件，用于URL处理
#include <QPainter>             // 包含QPainter类头文件，用于绘制标记点
#include <QPen>                 // 包含QPen类头文件，用于画笔设置
#include <QBrush>               // 包含QBrush类头文件，用于画刷设置
#include <QFont>                // 包含QFont类头文件，用于字体设置
#include <QWheelEvent>          // 包含QWheelEvent类头文件，用于鼠标滚轮事件

/**
 * @class MiniMapDialog
 * @brief 小地图弹窗类
 * @details 该类继承自QDialog，实现了一个小型地图显示窗口，用于显示目的地位置。
 *          支持以下功能：
 *          - 加载高德地图瓦片并合并显示
 *          - 在地图上绘制目的地标记点
 *          - 鼠标滚轮缩放地图
 *          - 鼠标拖动平移地图
 *          - 双击还原地图到初始状态
 */
class MiniMapDialog : public QDialog
{
    Q_OBJECT  // Qt元对象系统宏，使类支持信号槽机制

public:
    /**
     * @brief 构造函数
     * @param parent 父窗口指针，默认为nullptr
     */
    explicit MiniMapDialog(QWidget *parent = nullptr);
    
    /**
     * @brief 析构函数
     */
    ~MiniMapDialog();
    
    /**
     * @brief 设置地图位置
     * @param lon 经度
     * @param lat 纬度
     */
    void setPosition(double lon, double lat);
    
    /**
     * @brief 更新地图位置
     * @param lon 新的经度
     * @param lat 新的纬度
     */
    void updatePosition(double lon, double lat);
    
protected:
    /**
     * @brief 重写鼠标滚轮事件
     * @details 处理鼠标滚轮事件，实现地图缩放功能
     * @param event 滚轮事件对象
     */
    void wheelEvent(QWheelEvent *event) override;
    
    /**
     * @brief 重写鼠标按下事件
     * @details 处理鼠标按下事件，开始地图拖动
     * @param event 鼠标事件对象
     */
    void mousePressEvent(QMouseEvent *event) override;
    
    /**
     * @brief 重写鼠标移动事件
     * @details 处理鼠标移动事件，实现地图拖动
     * @param event 鼠标事件对象
     */
    void mouseMoveEvent(QMouseEvent *event) override;
    
    /**
     * @brief 重写鼠标释放事件
     * @details 处理鼠标释放事件，结束地图拖动
     * @param event 鼠标事件对象
     */
    void mouseReleaseEvent(QMouseEvent *event) override;
    
    /**
     * @brief 重写鼠标双击事件
     * @details 处理鼠标双击事件，还原地图到初始状态
     * @param event 鼠标事件对象
     */
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    
private slots:
    /**
     * @brief 地图图片加载完成槽函数
     * @details 处理地图图片加载完成后的逻辑
     * @param reply 网络响应对象
     */
    void onMapImageLoaded(QNetworkReply *reply);
    
public:
    /**
     * @brief 设置目的地坐标
     * @param lon 目的地经度
     * @param lat 目的地纬度
     */
    void setDestination(double lon, double lat);
    
private:
    QLabel *mapLabel;              ///< 地图显示标签，用于显示地图图片
    QLabel *coordLabel;            ///< 坐标信息标签，显示当前经纬度和缩放比例
    QNetworkAccessManager *networkManager; ///< 网络访问管理器，用于加载地图瓦片
    
    double currentLon;             ///< 当前经度
    double currentLat;             ///< 当前纬度
    double destLon;                ///< 目的地经度
    double destLat;                ///< 目的地纬度
    
    bool mapLoading;               ///< 地图加载状态标志：true=正在加载，false=未加载
    int currentRequestId;          ///< 当前请求ID，用于避免旧回调覆盖新结果
    
    QPixmap originalPixmap;        ///< 原始地图图片，用于缩放和拖动
    double currentScale;           ///< 当前缩放比例
    bool isDragging;               ///< 是否正在拖动地图
    QPoint lastMousePos;           ///< 上一次鼠标位置，用于计算拖动偏移
    QPoint mapOffset;              ///< 地图拖动偏移量
    
    double markerX;                ///< 标记点在原始图片中的X坐标
    double markerY;                ///< 标记点在原始图片中的Y坐标
    
    /**
     * @brief 将经纬度转换为瓦片坐标
     * @param lon 经度
     * @param lat 纬度
     * @param zoom 缩放级别
     * @param tileX 输出参数，瓦片X坐标
     * @param tileY 输出参数，瓦片Y坐标
     */
    void latLngToTile(double lon, double lat, int zoom, int &tileX, int &tileY);
    
    /**
     * @brief 加载地图图片
     * @details 通过高德地图瓦片API加载3x3的地图瓦片并合并成一张完整图片
     * @param lon 中心点经度
     * @param lat 中心点纬度
     */
    void loadMapImage(double lon, double lat);
    
    /**
     * @brief 更新地图显示
     * @details 根据当前缩放比例和偏移量更新地图显示
     */
    void updateMapDisplay();
};

#endif // MINIMAPDIALOG_H  // 结束头文件保护宏