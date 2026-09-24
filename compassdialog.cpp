#include "compassdialog.h"
#include <QPainter>
#include <QPen>
#include <QBrush>
#include <cmath>
#include <QPolygonF>
#include <QPointF>
#include <QShowEvent>
#include <QHideEvent>

CompassDialog::CompassDialog(QWidget *parent)
    : QDialog(parent), currentAngle(0)
{
    setWindowTitle("指南针");
    setFixedSize(300, 300);
    setStyleSheet("background: rgba(10, 20, 40, 0.95); border: 2px solid #00ffff; border-radius: 10px;");

    animationTimer = new QTimer(this);
    connect(animationTimer, &QTimer::timeout, this, &CompassDialog::updateAnimation);
}

CompassDialog::~CompassDialog()
{
    animationTimer->stop();
}

void CompassDialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    animationTimer->start(50);
}

void CompassDialog::hideEvent(QHideEvent *event)
{
    QDialog::hideEvent(event);
    animationTimer->stop();
}

void CompassDialog::setDirection(double angle)
{
    currentAngle = angle;
}

void CompassDialog::updateAnimation()
{
    currentAngle += 0.5;
    if (currentAngle >= 360) currentAngle -= 360;
    update();
}

void CompassDialog::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    int centerX = width() / 2;
    int centerY = height() / 2;
    int radius = qMin(width(), height()) / 2 - 20;

    painter.translate(centerX, centerY);

    QPen borderPen(QColor("#00ffff"), 3);
    painter.setPen(borderPen);
    painter.drawEllipse(-radius, -radius, radius * 2, radius * 2);

    for (int i = 0; i < 360; i += 10) {
        double angle = i * M_PI / 180;
        int innerRadius = radius - 15;
        int outerRadius = radius;
        if (i % 90 == 0) {
            innerRadius = radius - 25;
            painter.setPen(QPen(QColor("#00ffff"), 3));
        } else if (i % 30 == 0) {
            innerRadius = radius - 20;
            painter.setPen(QPen(QColor("#aaffff"), 2));
        } else {
            painter.setPen(QPen(QColor("#66aaaa"), 1));
        }
        double x1 = cos(angle) * innerRadius;
        double y1 = -sin(angle) * innerRadius;
        double x2 = cos(angle) * outerRadius;
        double y2 = -sin(angle) * outerRadius;
        painter.drawLine(QPointF(x1, y1), QPointF(x2, y2));
    }

    QString directions[] = {"N", "E", "S", "W"};
    QFont font("Arial", 14, QFont::Bold);
    painter.setFont(font);
    painter.setPen(QColor("#00ffff"));
    for (int i = 0; i < 4; i++) {
        double angle = i * 90 * M_PI / 180;
        int textRadius = radius - 35;
        double x = cos(angle) * textRadius;
        double y = -sin(angle) * textRadius;
        QRect textRect(-15, -15, 30, 30);
        textRect.moveCenter(QPoint(x, y));
        painter.drawText(textRect, Qt::AlignCenter, directions[i]);
    }

    painter.save();
    painter.rotate(-currentAngle);

    QPen needlePen(QColor("#ff4444"), 3);
    painter.setPen(needlePen);
    painter.drawLine(QPointF(0, 0), QPointF(0, -radius + 35));

    QPolygonF arrow;
    arrow << QPointF(0, -radius + 25)
          << QPointF(-10, -radius + 45)
          << QPointF(0, -radius + 38)
          << QPointF(10, -radius + 45);

    QBrush arrowBrush(QColor("#ff4444"));
    painter.setBrush(arrowBrush);
    painter.setPen(QPen(QColor("#ff4444"), 2));
    painter.drawPolygon(arrow);

    QBrush centerBrush(QColor("#00ffff"));
    painter.setBrush(centerBrush);
    painter.drawEllipse(-6, -6, 12, 12);

    painter.restore();
}