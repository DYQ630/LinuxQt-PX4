#include "rotationdial.h"
#include <QPainter>
#include <QPainterPath>
#include <QtMath>

RotationDial::RotationDial(QWidget *parent)
    : QWidget(parent)
    , selectedAngle(0)
    , hoverSector(-1)
{
    setFixedSize(200, 200);
    setCursor(Qt::PointingHandCursor);
    setMouseTracking(true);
}

int RotationDial::getSelectedAngle() const
{
    return selectedAngle;
}

int RotationDial::angleFromIndex(int index) const
{
    switch (index) {
    case 0: return 90;    // 右上：90°旋转
    case 1: return 180;   // 右下：180°旋转
    case 2: return 270;   // 左下：270°旋转
    case 3: return 360;   // 左上：360°旋转
    default: return 0;
    }
}

/**
 * @brief 获取指定点对应的扇形索引
 * @details 角度约定：0°在正上方，顺时针为正
 *          扇形0: 0°-90° (右上)
 *          扇形1: 90°-180° (右下)
 *          扇形2: 180°-270° (左下)
 *          扇形3: 270°-360° (左上)
 */
int RotationDial::getSectorIndex(const QPoint &pos) const
{
    QPoint center(width() / 2, height() / 2);
    double radius = width() / 2.0;
    double dist = qSqrt(qPow(pos.x() - center.x(), 2) + qPow(pos.y() - center.y(), 2));

    if (dist > radius || dist < radius * 0.2) {
        return -1;
    }

    // atan2(dy, dx) 在屏幕坐标系(y向下)中: 0°在3点钟方向
    double angleDeg = qAtan2(pos.y() - center.y(), pos.x() - center.x()) * 180.0 / M_PI;

    // 转换为"0°在正上方，顺时针为正"
    double normalized = angleDeg + 90.0;

    while (normalized >= 360.0) normalized -= 360.0;
    while (normalized < 0.0) normalized += 360.0;

    int index = static_cast<int>(normalized / 90.0);
    if (index >= 4) index = 0;

    return index;
}

/**
 * @brief 从"上方顺时针"角度计算屏幕坐标
 */
static QPointF angleToPos(double angleFromTop, double r, double cx, double cy)
{
    double rad = qDegreesToRadians(angleFromTop);
    // angleFromTop: 0°=上方, 顺时针为正
    // 转换: x = cx + r*sin(angle), y = cy - r*cos(angle)
    // 因为屏幕y轴向下, 顺时针方向的y变化与标准数学相反
    return QPointF(cx + r * qSin(rad), cy - r * qCos(rad));
}

void RotationDial::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    int side = qMin(width(), height());
    double radius = side / 2.0 - 5;
    double innerRadius = radius * 0.35;
    double cx = width() / 2.0;
    double cy = height() / 2.0;

    // 四个扇形: 0=右上(0-90°), 1=右下(90-180°), 2=左下(180-270°), 3=左上(270-360°)
    double ourStart[4] = {0.0, 90.0, 180.0, 270.0};
    const double ourSpan = 90.0;

    QString labels[4] = {"90°", "180°", "270°", "360°"};

    const int steps = 20;  // 每个扇形的细分步数，越大越平滑

    for (int i = 0; i < 4; i++) {
        int angle = angleFromIndex(i);
        bool isSelected = (selectedAngle == angle);
        bool isHovered = (hoverSector == i);

        QColor fillColor, borderColor, textColor;

        if (isSelected) {
            fillColor = QColor(200, 30, 30, 200);
            borderColor = QColor(255, 80, 80);
            textColor = QColor(255, 200, 200);
        } else if (isHovered) {
            fillColor = QColor(0, 100, 150, 180);
            borderColor = QColor(0, 200, 255);
            textColor = QColor(150, 240, 255);
        } else {
            fillColor = QColor(0, 40, 70, 180);
            borderColor = QColor(0, 180, 220);
            textColor = QColor(0, 220, 255);
        }

        double startAngle = ourStart[i];
        double endAngle = ourStart[i] + ourSpan;

        // 手动构建环形扇区路径（用细线段近似圆弧）
        QPainterPath path;
        double stepSize = ourSpan / steps;

        // 外弧: 从startAngle到endAngle
        for (int s = 0; s <= steps; s++) {
            double a = startAngle + s * stepSize;
            QPointF pt = angleToPos(a, radius, cx, cy);
            if (s == 0) path.moveTo(pt);
            else path.lineTo(pt);
        }

        // 内弧: 从endAngle回到startAngle(反向)
        for (int s = steps; s >= 0; s--) {
            double a = startAngle + s * stepSize;
            QPointF pt = angleToPos(a, innerRadius, cx, cy);
            path.lineTo(pt);
        }

        path.closeSubpath();

        painter.setBrush(QBrush(fillColor));
        painter.setPen(QPen(borderColor, 2));
        painter.drawPath(path);

        // 分割线
        painter.setPen(QPen(QColor(0, 200, 255, 100), 1));
        QPointF p1 = angleToPos(startAngle, innerRadius, cx, cy);
        QPointF p2 = angleToPos(startAngle, radius, cx, cy);
        painter.drawLine(p1, p2);

        // 标签文字
        double midAngle = startAngle + ourSpan / 2.0;
        QPointF textPos = angleToPos(midAngle, (radius + innerRadius) / 2.0, cx, cy);

        painter.setPen(textColor);
        QFont font;
        font.setBold(true);
        font.setPointSize(11);
        painter.setFont(font);

        int labelWidth = fontMetrics().horizontalAdvance(labels[i]);
        painter.drawText(QPoint(textPos.x() - labelWidth / 2, textPos.y() + 5), labels[i]);
    }

    // 中心圆
    painter.setBrush(QBrush(QColor(13, 26, 42, 255)));
    painter.setPen(QPen(QColor(0, 200, 255), 2));
    painter.drawEllipse(QPointF(cx, cy), innerRadius, innerRadius);

    // 中心文字
    painter.setPen(QColor(0, 220, 255));
    QFont centerFont;
    centerFont.setBold(true);
    centerFont.setPointSize(10);
    painter.setFont(centerFont);

    if (selectedAngle > 0) {
        painter.drawText(QRectF(cx - 30, cy - 15, 60, 30),
                         Qt::AlignCenter, QString("%1°").arg(selectedAngle));
    } else {
        painter.drawText(QRectF(cx - 30, cy - 15, 60, 30),
                         Qt::AlignCenter, QString::fromUtf8("旋转"));
    }
}

void RotationDial::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        int sector = getSectorIndex(event->pos());
        if (sector >= 0) {
            int angle = angleFromIndex(sector);
            selectedAngle = angle;
            hoverSector = sector;
            update();
            emit sectorClicked(angle);
        }
    }
    QWidget::mousePressEvent(event);
}

void RotationDial::mouseMoveEvent(QMouseEvent *event)
{
    int sector = getSectorIndex(event->pos());
    if (sector != hoverSector) {
        hoverSector = sector;
        update();
    }
    QWidget::mouseMoveEvent(event);
}

void RotationDial::leaveEvent(QEvent *event)
{
    if (hoverSector != -1) {
        hoverSector = -1;
        update();
    }
    QWidget::leaveEvent(event);
}
