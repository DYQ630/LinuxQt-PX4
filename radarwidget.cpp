/**
 * @file radarwidget.cpp
 * @brief 雷达扫描可视化控件实现文件
 * @details 实现RadarWidget类的所有成员函数，包括雷达背景绘制、扫描线动画、回波目标显示等功能。
 */

#include "radarwidget.h"  // 包含RadarWidget类的头文件
#include <cmath>           // 包含数学运算头文件，用于三角函数计算
#include <QResizeEvent>    // 包含尺寸事件头文件
#include <QRandomGenerator>  // 包含随机数生成头文件

/**
 * @brief RadarWidget构造函数
 * @details 初始化雷达控件的成员变量，创建扫描定时器并启动扫描动画
 * @param parent 父窗口指针
 */
RadarWidget::RadarWidget(QWidget *parent)
    : QWidget(parent)  // 调用父类QWidget的构造函数
{
    // 初始化扫描角度为0（弧度），即指向正上方
    m_scanAngle = 0.0;
    
    // 设置扫描速度为每秒旋转2弧度（约114度/秒）
    m_scanSpeed = 2.0;
    
    // 初始化绘制参数（稍后在resizeEvent中更新）
    m_centerX = 0;
    m_centerY = 0;
    m_radius = 0;
    
    // 生成初始随机目标
    generateRandomTargets();
    
    // 创建扫描定时器，父对象为当前控件
    m_scanTimer = new QTimer(this);
    
    // 设置定时器间隔为33毫秒（约30帧/秒），保证动画流畅
    m_scanTimer->setInterval(33);
    
    // 连接定时器超时信号到扫描更新槽函数
    connect(m_scanTimer, &QTimer::timeout, this, &RadarWidget::onScanTimerTimeout);
    
    // 启动扫描动画
    startScan();
    
    // 设置控件最小尺寸，保证雷达有足够的显示空间
    setMinimumSize(150, 150);
}

/**
 * @brief RadarWidget析构函数
 * @details 清理资源，停止扫描定时器
 */
RadarWidget::~RadarWidget()
{
    // 停止扫描定时器
    if (m_scanTimer) {
        m_scanTimer->stop();  // 停止定时器
    }
}

/**
 * @brief 设置雷达扫描速度
 * @details 根据输入的度数/秒转换为弧度/秒并更新内部速度
 * @param speedDegreesPerSecond 扫描线旋转速度（度/秒）
 */
void RadarWidget::setScanSpeed(double speedDegreesPerSecond)
{
    // 将度/秒转换为弧度/秒（1度 = π/180弧度）
    m_scanSpeed = speedDegreesPerSecond * M_PI / 180.0;
}

/**
 * @brief 启动雷达扫描动画
 * @details 启动扫描定时器，开始扫描线旋转
 */
void RadarWidget::startScan()
{
    // 检查定时器是否存在且未运行
    if (m_scanTimer && !m_scanTimer->isActive()) {
        m_scanTimer->start();  // 启动定时器
    }
}

/**
 * @brief 停止雷达扫描动画
 * @details 停止扫描定时器，暂停扫描线旋转
 */
void RadarWidget::stopScan()
{
    // 检查定时器是否存在且正在运行
    if (m_scanTimer && m_scanTimer->isActive()) {
        m_scanTimer->stop();  // 停止定时器
    }
}

/**
 * @brief 绘制事件处理函数
 * @details 绘制雷达的所有视觉元素：背景、扫描线、目标点
 * @param event 绘制事件参数
 */
void RadarWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);  // 未使用的参数，消除编译器警告
    
    // 创建QPainter对象，用于在当前控件上绘图
    QPainter painter(this);
    
    // 启用抗锯齿，使圆形和线条绘制更平滑
    painter.setRenderHint(QPainter::Antialiasing, true);
    
    // 按顺序绘制各个元素：背景 → 扫描线 → 目标点
    drawRadarBackground(painter);  // 绘制雷达背景
    drawScanLine(painter);          // 绘制扫描线
    drawTargets(painter);           // 绘制回波目标点
}

/**
 * @brief 尺寸事件处理函数
 * @details 当控件尺寸变化时，重新计算雷达中心坐标和半径
 * @param event 尺寸事件参数
 */
void RadarWidget::resizeEvent(QResizeEvent *event)
{
    // 调用父类的尺寸事件处理
    QWidget::resizeEvent(event);
    
    // 计算雷达中心坐标（控件中心点）
    m_centerX = width() / 2;
    m_centerY = height() / 2;
    
    // 计算雷达半径（取控件宽高中较小值的一半，减去边距）
    m_radius = qMin(width(), height()) / 2 - 2;
}

/**
 * @brief 定时器超时槽函数
 * @details 更新扫描线角度，处理角度循环（0~2π），触发重绘
 */
void RadarWidget::onScanTimerTimeout()
{
    // 根据时间间隔和扫描速度更新角度
    // 时间间隔为33毫秒（0.033秒）
    m_scanAngle += m_scanSpeed * 0.033;
    
    // 角度循环：当角度超过2π时重置为0
    // 2π ≈ 6.2832弧度（360度）
    if (m_scanAngle > 2.0 * M_PI) {
        m_scanAngle -= 2.0 * M_PI;  // 减去一个完整圆周
    }
    
    // 触发重绘，更新雷达画面
    update();
}

/**
 * @brief 绘制雷达背景
 * @details 绘制圆形扫描区域、量程环、方位线等背景元素
 * @param painter QPainter绘图对象引用
 */
void RadarWidget::drawRadarBackground(QPainter &painter)
{
    // 1. 绘制雷达外圈（带发光效果的渐变圆形）
    QRadialGradient outerGradient(m_centerX, m_centerY, m_radius);
    outerGradient.setColorAt(0.0, QColor(0, 50, 80));          // 中心：深蓝色
    outerGradient.setColorAt(0.7, QColor(0, 30, 50));          // 外圈：深色
    outerGradient.setColorAt(0.95, QColor(0, 80, 120, 200));   // 边缘：半透明蓝色
    outerGradient.setColorAt(1.0, QColor(0, 0, 0, 0));        // 外缘：透明
    
    painter.setBrush(outerGradient);  // 设置渐变填充
    painter.setPen(Qt::NoPen);        // 不绘制边框
    painter.drawEllipse(m_centerX - m_radius, m_centerY - m_radius,
                       m_radius * 2, m_radius * 2);  // 绘制圆形
    
    // 2. 绘制内圈背景（暗色填充）
    painter.setBrush(QColor(0, 15, 30));  // 设置深暗青色填充
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(m_centerX - m_radius + 2, m_centerY - m_radius + 2,
                       (m_radius - 2) * 2, (m_radius - 2) * 2);
    
    // 3. 绘制外圈边框（发光效果）
    painter.setPen(QPen(QColor(0, 200, 255, 200), 2));  // 青色边框，带透明度
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(m_centerX - m_radius, m_centerY - m_radius,
                       m_radius * 2, m_radius * 2);
    
    // 4. 绘制量程环（同心圆环，5个等间距环）
    for (int i = 1; i <= 5; ++i) {
        double ringRadius = m_radius * i / 5.0;  // 计算当前环的半径
        painter.setPen(QPen(QColor(0, 150, 200, 100), 1));  // 半透明青色细线
        painter.drawEllipse(m_centerX - ringRadius, m_centerY - ringRadius,
                           ringRadius * 2, ringRadius * 2);
    }
    
    // 5. 绘制方位线（4条十字线，表示N/E/S/W方位）
    painter.setPen(QPen(QColor(0, 150, 200, 120), 1));  // 半透明青色
    // 垂直线（南北方向）
    painter.drawLine(m_centerX, m_centerY - m_radius, m_centerX, m_centerY + m_radius);
    // 水平线（东西方向）
    painter.drawLine(m_centerX - m_radius, m_centerY, m_centerX + m_radius, m_centerY);
    
    // 6. 绘制对角方位线（两条对角线）
    painter.setPen(QPen(QColor(0, 150, 200, 60), 1, Qt::DashLine));  // 虚线，更低透明度
    // 对角线1（东北-西南）
    painter.drawLine(
        m_centerX - m_radius * cos(M_PI_4), m_centerY - m_radius * sin(M_PI_4),
        m_centerX + m_radius * cos(M_PI_4), m_centerY + m_radius * sin(M_PI_4));
    // 对角线2（西北-东南）
    painter.drawLine(
        m_centerX - m_radius * cos(M_PI_4), m_centerY + m_radius * sin(M_PI_4),
        m_centerX + m_radius * cos(M_PI_4), m_centerY - m_radius * sin(M_PI_4));
    
    // 7. 绘制中心点（小圆形表示雷达位置）
    painter.setBrush(QColor(0, 255, 200));  // 亮青色填充
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(m_centerX - 3, m_centerY - 3, 6, 6);  // 6x6像素的中心点
    
    // 8. 绘制方位标签（N/S/E/W）
    painter.setPen(QColor(0, 220, 255, 200));  // 亮青色文字
    QFont labelFont = font();
    labelFont.setPointSize(7);  // 设置字体大小为7点
    painter.setFont(labelFont);
    // 北（N）标签：正上方
    painter.drawText(m_centerX - 5, m_centerY - m_radius + 12, "N");
    // 南（S）标签：正下方
    painter.drawText(m_centerX - 5, m_centerY + m_radius - 4, "S");
    // 东（E）标签：右侧
    painter.drawText(m_centerX + m_radius - 8, m_centerY + 4, "E");
    // 西（W）标签：左侧
    painter.drawText(m_centerX - m_radius + 2, m_centerY + 4, "W");
}

/**
 * @brief 绘制扫描线
 * @details 绘制带渐变拖尾效果的旋转扫描线，模拟真实雷达扫描
 * @param painter QPainter绘图对象引用
 */
void RadarWidget::drawScanLine(QPainter &painter)
{
    // 扫描线长度为雷达半径的95%
    double lineLength = m_radius * 0.95;
    
    // 计算扫描线终点坐标
    // 注意：Qt坐标系中Y轴向下，所以sin函数用负号
    double endX = m_centerX + lineLength * sin(m_scanAngle);
    double endY = m_centerY - lineLength * cos(m_scanAngle);
    
    // 1. 绘制扫描拖尾（扇形渐变，模拟扫描线余辉）
    // 拖尾角度为45度（π/4弧度）
    double tailAngle = M_PI / 4.0;
    
    // 创建扇形渐变路径
    QPainterPath tailPath;
    tailPath.moveTo(m_centerX, m_centerY);  // 起点：雷达中心
    
    // 绘制扇形起始边（扫描线当前角度）
    tailPath.lineTo(endX, endY);
    
    // 绘制扇形弧（从当前角度回溯45度）
    // Qt中角度以度数表示，0度指向3点钟方向
    // 转换到Qt坐标系：Qt角度 = 我们的角度 - 90度（因为我们的0度指向12点钟方向）
    double qtAngle1 = m_scanAngle * 180.0 / M_PI - 90.0;  // 当前扫描线角度
    
    // 添加弧形到路径
    tailPath.arcTo(QRectF(m_centerX - lineLength, m_centerY - lineLength,
                          lineLength * 2, lineLength * 2),
                   qtAngle1, -tailAngle * 180.0 / M_PI);  // 负号表示逆时针
    
    tailPath.closeSubpath();  // 闭合路径
    
    // 创建扇形渐变（从中心向外，颜色由亮变透明）
    QRadialGradient tailGradient(m_centerX, m_centerY, lineLength);
    tailGradient.setColorAt(0.0, QColor(0, 255, 200, 150));    // 中心：半透明青色
    tailGradient.setColorAt(0.3, QColor(0, 200, 150, 100));    // 中圈：淡青色
    tailGradient.setColorAt(0.7, QColor(0, 150, 100, 50));     // 外圈：淡绿色
    tailGradient.setColorAt(1.0, QColor(0, 100, 50, 0));       // 末端：透明
    
    painter.setBrush(tailGradient);  // 设置渐变填充
    painter.setPen(Qt::NoPen);        // 不绘制边框
    painter.drawPath(tailPath);       // 绘制扇形路径
    
    // 2. 绘制扫描线本身（明亮线条）
    // 扫描线使用亮青色，带发光效果
    QPen scanPen(QColor(0, 255, 220, 255));  // 亮青色
    scanPen.setWidth(2);  // 线宽为2像素
    painter.setPen(scanPen);
    painter.drawLine(m_centerX, m_centerY, static_cast<int>(endX), static_cast<int>(endY));
    
    // 3. 绘制扫描线头部发光点
    painter.setBrush(QColor(0, 255, 220, 200));  // 半透明亮青色
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(static_cast<int>(endX) - 4, static_cast<int>(endY) - 4, 8, 8);  // 8x8像素发光点
}

/**
 * @brief 绘制回波目标点
 * @details 根据扫描线位置，高亮显示被扫描到的目标点
 * @param painter QPainter绘图对象引用
 */
void RadarWidget::drawTargets(QPainter &painter)
{
    // 遍历所有目标点
    for (const RadarTarget &target : m_targets) {
        // 计算目标在控件上的实际坐标
        double targetX = m_centerX + m_radius * target.distance * sin(target.angle);
        double targetY = m_centerY - m_radius * target.distance * cos(target.angle);
        
        // 检查目标是否在当前扫描线的扫描范围内
        bool inSweep = isTargetInSweepRange(target.angle);
        
        if (inSweep) {
            // 目标在扫描范围内，绘制高亮回波（带发光效果）
            int size = static_cast<int>(4 + target.intensity * 6);  // 目标大小4~10像素
            
            // 绘制外发光层
            painter.setBrush(QColor(0, 255, 200, 80));  // 半透明青色外发光
            painter.setPen(Qt::NoPen);
            painter.drawEllipse(
                static_cast<int>(targetX) - size - 2,
                static_cast<int>(targetY) - size - 2,
                (size + 2) * 2, (size + 2) * 2);
            
            // 绘制目标主体
            painter.setBrush(QColor(100, 255, 220, 255));  // 亮青色主体
            painter.setPen(QPen(QColor(200, 255, 240), 1));  // 白色边框
            painter.drawEllipse(
                static_cast<int>(targetX) - size,
                static_cast<int>(targetY) - size,
                size * 2, size * 2);
            
        } else {
            // 目标不在扫描范围内，绘制暗点（残留回波）
            int size = static_cast<int>(2 + target.intensity * 3);  // 较小的残留点
            
            painter.setBrush(QColor(0, 150, 120, 60));  // 暗绿色残留点
            painter.setPen(Qt::NoPen);
            painter.drawEllipse(
                static_cast<int>(targetX) - size,
                static_cast<int>(targetY) - size,
                size * 2, size * 2);
        }
    }
}

/**
 * @brief 生成随机目标
 * @details 在雷达扫描区域内随机生成模拟的雷达目标
 */
void RadarWidget::generateRandomTargets()
{
    // 清除现有目标
    m_targets.clear();
    
    // 生成5~10个随机目标
    int targetCount = QRandomGenerator::global()->bounded(5, 11);
    
    for (int i = 0; i < targetCount; ++i) {
        RadarTarget target;
        
        // 随机角度：0~2π（0~360度）
        // 使用generateDouble()生成0~1的随机数，再映射到目标范围
        target.angle = QRandomGenerator::global()->generateDouble() * 2.0 * M_PI;
        
        // 随机距离：0.1~0.9（避免太靠近中心和边缘）
        // 公式：min + random * (max - min)
        target.distance = 0.1 + QRandomGenerator::global()->generateDouble() * 0.8;
        
        // 随机强度：0.5~1.0（确保目标有足够的可见度）
        target.intensity = 0.5 + QRandomGenerator::global()->generateDouble() * 0.5;
        
        // 添加到目标列表
        m_targets.append(target);
    }
}

/**
 * @brief 检查目标是否在扫描线范围内
 * @details 判断目标角度与当前扫描线角度的差值是否在扫描范围内
 * @param targetAngle 目标角度（弧度）
 * @return true=在扫描范围内，false=不在扫描范围内
 */
bool RadarWidget::isTargetInSweepRange(double targetAngle) const
{
    // 扫描范围为45度（π/4弧度），与拖尾角度匹配
    double sweepRange = M_PI / 4.0;
    
    // 计算角度差（处理环形角度）
    double angleDiff = targetAngle - m_scanAngle;
    
    // 将角度差规范化到 -π ~ π 范围
    while (angleDiff > M_PI) {
        angleDiff -= 2.0 * M_PI;  // 减去2π
    }
    while (angleDiff < -M_PI) {
        angleDiff += 2.0 * M_PI;  // 加上2π
    }
    
    // 检查目标是否在扫描线前方的扇形范围内（负角度差表示在扫描线前方）
    return (angleDiff < 0 && angleDiff > -sweepRange);
}
