/**
 * @file radarwidget.h
 * @brief 雷达扫描可视化控件头文件
 * @details 该类实现了一个动态雷达扫描控件，包含旋转扫描线、回波目标点和扫描背景。
 *          通过QPainter绘制雷达圆形区域，模拟真实雷达的扫描效果。
 */

#ifndef RADARWIDGET_H      // 头文件保护宏，防止头文件被重复包含
#define RADARWIDGET_H      // 定义头文件保护宏

#include <QWidget>          // 包含QWidget类头文件，作为RadarWidget类的基类
#include <QTimer>           // 包含QTimer类头文件，用于驱动扫描线旋转动画
#include <QPainter>         // 包含QPainter类头文件，用于绘制雷达图形
#include <QPointF>          // 包含QPointF类头文件，用于存储浮点数坐标

/**
 * @struct RadarTarget
 * @brief 雷达目标结构体
 * @details 存储单个雷达目标的位置和强度信息
 */
struct RadarTarget {
    double angle;    ///< 目标角度（弧度），0度指向正上方
    double distance; ///< 目标距离（相对于雷达中心的比例值，0.0~1.0）
    double intensity; ///< 目标回波强度（0.0~1.0）
};

/**
 * @class RadarWidget
 * @brief 雷达扫描可视化控件类
 * @details 该类继承自QWidget，实现动态雷达扫描效果：
 *          - 圆形扫描区域带渐变背景
 *          - 旋转扫描线（带拖尾渐隐效果）
 *          - 随机生成的回波目标点
 *          - 扫描线扫过时目标点高亮显示
 */
class RadarWidget : public QWidget  // 定义RadarWidget类，继承自QWidget基类
{
    Q_OBJECT  // Qt元对象系统宏，使类支持信号槽机制

public:
    /**
     * @brief 构造函数
     * @param parent 父窗口指针，默认为nullptr
     */
    explicit RadarWidget(QWidget *parent = nullptr);  // 声明构造函数，接收父窗口指针参数

    /**
     * @brief 析构函数
     */
    ~RadarWidget();  // 声明析构函数

    /**
     * @brief 设置雷达扫描速度
     * @param speedDegreesPerSecond 扫描线旋转速度（度/秒）
     */
    void setScanSpeed(double speedDegreesPerSecond);  // 声明设置扫描速度函数

    /**
     * @brief 启动雷达扫描动画
     */
    void startScan();  // 声明启动扫描函数

    /**
     * @brief 停止雷达扫描动画
     */
    void stopScan();  // 声明停止扫描函数

protected:
    /**
     * @brief 重写绘制事件
     * @details 绘制雷达背景、扫描线、回波点等所有视觉元素
     * @param event 绘制事件参数
     */
    void paintEvent(QPaintEvent *event) override;  // 重写绘制事件处理函数

    /**
     * @brief 重写尺寸事件
     * @details 当控件尺寸变化时调整绘制参数
     * @param event 尺寸事件参数
     */
    void resizeEvent(QResizeEvent *event) override;  // 重写尺寸事件处理函数

private slots:
    /**
     * @brief 定时器超时槽函数
     * @details 更新扫描线角度并触发重绘
     */
    void onScanTimerTimeout();  // 声明扫描定时器超时槽函数

private:
    /**
     * @brief 绘制雷达背景
     * @param painter QPainter绘图对象引用
     */
    void drawRadarBackground(QPainter &painter);  // 声明绘制雷达背景函数

    /**
     * @brief 绘制扫描线
     * @param painter QPainter绘图对象引用
     */
    void drawScanLine(QPainter &painter);  // 声明绘制扫描线函数

    /**
     * @brief 绘制回波目标点
     * @param painter QPainter绘图对象引用
     */
    void drawTargets(QPainter &painter);  // 声明绘制目标点函数

    /**
     * @brief 生成随机目标
     * @details 在雷达区域内随机生成模拟目标
     */
    void generateRandomTargets();  // 声明生成随机目标函数

    /**
     * @brief 检查目标是否在扫描线附近
     * @param targetAngle 目标角度
     * @return 是否在扫描线角度范围内
     */
    bool isTargetInSweepRange(double targetAngle) const;  // 声明检查目标是否在扫描范围内函数

    QTimer *m_scanTimer;          ///< 扫描定时器，用于驱动扫描线旋转
    double m_scanAngle;           ///< 当前扫描线角度（弧度）
    double m_scanSpeed;           ///< 扫描线旋转速度（弧度/秒）
    QVector<RadarTarget> m_targets;  ///< 雷达目标列表

    // 绘制参数缓存
    int m_centerX;                ///< 雷达中心X坐标
    int m_centerY;                ///< 雷达中心Y坐标
    int m_radius;                 ///< 雷达扫描半径
};

#endif // RADARWIDGET_H  // 结束头文件保护宏
