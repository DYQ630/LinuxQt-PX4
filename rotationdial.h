#ifndef ROTATIONDIAL_H
#define ROTATIONDIAL_H

#include <QWidget>
#include <QMouseEvent>
#include <QPaintEvent>

/**
 * @class RotationDial
 * @brief 圆盘旋转选择控件
 * @details 一个圆形控件，被四个扇形按钮瓜分，分别对应：
 *          - 90°旋转
 *          - 180°旋转
 *          - 270°旋转
 *          - 360°旋转
 */
class RotationDial : public QWidget
{
    Q_OBJECT

public:
    explicit RotationDial(QWidget *parent = nullptr);

    /**
     * @brief 获取选中的旋转角度
     * @return 旋转角度（0=未选择，90/180/270/360=对应角度）
     */
    int getSelectedAngle() const;

signals:
    /**
     * @brief 扇形被点击信号
     * @param angle 对应的旋转角度（90/180/270/360）
     */
    void sectorClicked(int angle);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    int selectedAngle;  ///< 当前选中的角度（0=未选择）
    int hoverSector;    ///< 鼠标悬停的扇形索引（0-3，-1=无）

    /**
     * @brief 获取指定点对应的扇形索引
     * @param pos 相对于控件的坐标
     * @return 扇形索引（0-3），-1表示不在圆内
     */
    int getSectorIndex(const QPoint &pos) const;

    /**
     * @brief 根据扇形索引获取对应的角度
     * @param index 扇形索引（0-3）
     * @return 对应的旋转角度
     */
    int angleFromIndex(int index) const;
};

#endif // ROTATIONDIAL_H
