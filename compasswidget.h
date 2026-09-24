/**
 * @file compasswidget.h
 * @brief 指南针控件类头文件
 * @details 该类实现了一个科技风格的指南针控件，支持自动旋转动画，显示N/E/S/W四个方向。
 */

#ifndef COMPASSWIDGET_H     // 头文件保护宏，防止头文件被重复包含
#define COMPASSWIDGET_H     // 定义头文件保护宏

#include <QWidget>          // 包含QWidget类头文件，作为CompassWidget类的基类
#include <QTimer>           // 包含QTimer类头文件，用于动画定时器

/**
 * @class CompassWidget
 * @brief 指南针控件类
 * @details 该类继承自QWidget，实现了一个科技风格的指南针控件，
 *          包含以下功能：
 *          - 绘制圆形罗盘背景和刻度
 *          - 绘制N/E/S/W四个方向标签
 *          - 绘制红色旋转指针（带箭头）
 *          - 自动旋转动画效果
 */
class CompassWidget : public QWidget
{
    Q_OBJECT  // Qt元对象系统宏，使类支持信号槽机制

public:
    /**
     * @brief 构造函数
     * @param parent 父窗口指针，默认为nullptr
     */
    explicit CompassWidget(QWidget *parent = nullptr);
    
    /**
     * @brief 析构函数
     */
    ~CompassWidget();

protected:
    /**
     * @brief 重写绘制事件
     * @details 绘制指南针的所有元素：背景、刻度、方向标签、指针
     * @param event 绘制事件对象
     */
    void paintEvent(QPaintEvent *event) override;

private slots:
    /**
     * @brief 更新动画槽函数
     * @details 定时器触发时更新旋转角度并触发重绘
     */
    void updateAnimation();

private:
    QTimer *animationTimer;   ///< 动画定时器，用于驱动指针旋转
    double currentAngle;      ///< 当前旋转角度（度），范围0-360
};

#endif // COMPASSWIDGET_H  // 结束头文件保护宏