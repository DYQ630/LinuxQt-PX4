/**
 * @file compasswidget.cpp
 * @brief 指南针控件类实现文件
 * @details 该文件实现了CompassWidget类的所有成员函数，包括构造函数、析构函数、
 *          动画更新和绘制事件，实现了一个科技风格的旋转指南针效果。
 */

// 包含CompassWidget类的头文件
#include "compasswidget.h"
#include <QPainter>       // 包含QPainter类头文件，用于绘制
#include <QPen>           // 包含QPen类头文件，用于画笔设置
#include <QBrush>         // 包含QBrush类头文件，用于画刷设置
#include <cmath>          // 包含cmath库，用于数学计算（sin、cos等）
#include <QPolygonF>      // 包含QPolygonF类头文件，用于多边形绘制
#include <QPointF>        // 包含QPointF类头文件，用于浮点坐标点

/**
 * @brief CompassWidget类构造函数
 * @details 初始化控件大小、样式和成员变量，创建动画定时器并启动
 * @param parent 父窗口指针，默认为nullptr
 */
CompassWidget::CompassWidget(QWidget *parent)
    : QWidget(parent), currentAngle(0)  // 调用父类构造函数，初始化currentAngle为0
{
    setFixedSize(80, 80);              // 设置固定大小80x80像素（科技风格尺寸）
    setStyleSheet("background: transparent;");  // 设置背景透明，避免遮挡父控件背景

    // 创建动画定时器，50ms触发一次（20fps）
    animationTimer = new QTimer(this);
    // 连接定时器超时信号到动画更新槽函数
    connect(animationTimer, &QTimer::timeout, this, &CompassWidget::updateAnimation);
    animationTimer->start(50);         // 启动定时器，开始动画
}

/**
 * @brief CompassWidget类析构函数
 * @details 停止动画定时器，释放资源
 */
CompassWidget::~CompassWidget()
{
    animationTimer->stop();  // 停止动画定时器
}

/**
 * @brief 更新动画槽函数
 * @details 定时器触发时更新旋转角度（每次增加0.5度），超过360度时重置，触发重绘
 */
void CompassWidget::updateAnimation()
{
    currentAngle += 0.5;      // 每次增加0.5度
    if (currentAngle >= 360) currentAngle -= 360;  // 超过360度时减去360，保持在0-360范围内
    update();                 // 触发重绘事件
}

/**
 * @brief 重写绘制事件
 * @details 绘制指南针的所有元素：圆形边框、刻度线、方向标签（N/E/S/W）、红色旋转指针和中心点
 * @param event 绘制事件对象（未使用）
 */
void CompassWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);          // 未使用的参数，避免编译警告
    
    QPainter painter(this);   // 创建绘制器
    painter.setRenderHint(QPainter::Antialiasing);  // 启用抗锯齿，使绘制更平滑

    // ==================== 计算中心坐标和半径 ====================
    int centerX = width() / 2;         // 控件中心X坐标
    int centerY = height() / 2;        // 控件中心Y坐标
    int radius = qMin(width(), height()) / 2 - 8;  // 罗盘半径（减去8px边距）

    // ==================== 平移坐标系到中心 ====================
    painter.translate(centerX, centerY);  // 将原点移动到控件中心

    // ==================== 绘制圆形边框 ====================
    QPen borderPen(QColor("#00ffff"), 3);  // 创建青色边框画笔，宽度3px
    painter.setPen(borderPen);             // 设置画笔
    // 绘制圆形边框：圆心(0,0)，半径radius
    painter.drawEllipse(-radius, -radius, radius * 2, radius * 2);

    // ==================== 绘制刻度线 ====================
    // 每15度绘制一条刻度线，共24条
    for (int i = 0; i < 360; i += 15) {
        double angle = i * M_PI / 180;    // 将角度转换为弧度
        int innerRadius = radius - 10;    // 内圈半径
        int outerRadius = radius;         // 外圈半径
        
        // 根据角度设置刻度线样式
        if (i % 90 == 0) {
            // 90度倍数（N/E/S/W方向）：长刻度线，青色，宽度3px
            innerRadius = radius - 20;
            painter.setPen(QPen(QColor("#00ffff"), 3));
        } else if (i % 45 == 0) {
            // 45度倍数：中等刻度线，浅青色，宽度2px
            innerRadius = radius - 15;
            painter.setPen(QPen(QColor("#aaffff"), 2));
        } else {
            // 其他角度：短刻度线，淡青色，宽度1px
            painter.setPen(QPen(QColor("#66aaaa"), 1));
        }
        
        // 计算刻度线起点和终点坐标
        double x1 = cos(angle) * innerRadius;
        double y1 = -sin(angle) * innerRadius;  // 负号因为Y轴向下为正
        double x2 = cos(angle) * outerRadius;
        double y2 = -sin(angle) * outerRadius;
        
        // 绘制刻度线
        painter.drawLine(QPointF(x1, y1), QPointF(x2, y2));
    }

    // ==================== 绘制方向标签（N/E/S/W） ====================
    QString directions[] = {"N", "E", "S", "W"};  // 方向数组
    QFont font("Arial", 10, QFont::Bold);         // Arial字体跨平台兼容，10号，加粗
    painter.setFont(font);                         // 设置字体
    painter.setPen(QColor("#00ffff"));             // 青色文字
    
    // 遍历四个方向
    for (int i = 0; i < 4; i++) {
        double angle = i * 90 * M_PI / 180;       // 每个方向间隔90度
        int textRadius = radius - 25;              // 文字距离中心的半径
        
        // 计算文字位置
        double x = cos(angle) * textRadius;
        double y = -sin(angle) * textRadius;
        
        // 创建文字绘制区域（24x24像素，居中对齐）
        QRect textRect(-12, -12, 24, 24);
        textRect.moveCenter(QPoint(x, y));
        
        // 绘制方向文字
        painter.drawText(textRect, Qt::AlignCenter, directions[i]);
    }

    // ==================== 保存当前绘制状态 ====================
    painter.save();
    // 旋转坐标系（负号表示逆时针旋转，模拟指南针效果）
    painter.rotate(-currentAngle);

    // ==================== 绘制红色指针（带箭头） ====================
    // 绘制指针主干线
    QPen needlePen(QColor("#ff4444"), 3);  // 红色画笔，宽度3px
    painter.setPen(needlePen);              // 设置画笔
    // 从中心(0,0)向上绘制到(radius-25)位置
    painter.drawLine(QPointF(0, 0), QPointF(0, -radius + 25));

    // 绘制箭头头部（多边形）
    QPolygonF arrow;
    arrow << QPointF(0, -radius + 15)           // 箭头顶部
          << QPointF(-8, -radius + 32)          // 箭头左下
          << QPointF(0, -radius + 27)           // 箭头底部中心
          << QPointF(8, -radius + 32);          // 箭头右下

    QBrush arrowBrush(QColor("#ff4444"));       // 红色画刷
    painter.setBrush(arrowBrush);               // 设置画刷
    painter.setPen(QPen(QColor("#ff4444"), 2)); // 红色边框，宽度2px
    painter.drawPolygon(arrow);                 // 绘制箭头多边形

    // ==================== 绘制中心点 ====================
    QBrush centerBrush(QColor("#00ffff"));      // 青色画刷
    painter.setBrush(centerBrush);              // 设置画刷
    // 绘制10x10像素的中心点圆
    painter.drawEllipse(-5, -5, 10, 10);

    // ==================== 恢复绘制状态 ====================
    painter.restore();
}