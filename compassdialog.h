#ifndef COMPASSDIALOG_H
#define COMPASSDIALOG_H

#include <QDialog>
#include <QPainter>
#include <QTimer>
#include <QShowEvent>
#include <QHideEvent>

class CompassDialog : public QDialog
{
    Q_OBJECT
public:
    explicit CompassDialog(QWidget *parent = nullptr);
    ~CompassDialog();

    void setDirection(double angle);

private slots:
    void updateAnimation();

protected:
    void paintEvent(QPaintEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    double currentAngle;
    QTimer *animationTimer;
};

#endif // COMPASSDIALOG_H