/**
 * @file px4controller.h
 * @brief PX4 仿真控制器（基于 MAVSDK-C++ v3）
 * @details 封装与 PX4 SITL 的 UDP 通信，提供起飞/悬停/降落等高层接口，
 *          并通过 Qt 信号把遥测数据（位置、姿态、速度、电池、飞行状态）回传给主界面。
 *          所有 MAVSDK 阻塞调用都在内部工作线程执行，主线程仅触发控制指令。
 */

#ifndef PX4CONTROLLER_H
#define PX4CONTROLLER_H

#include <QObject>
#include <QString>
#include <QTimer>
#include <QThread>
#include <QMutex>
#include <QMutexLocker>
#include <memory>

// MAVSDK 头文件（需要完整类型定义）
#include <mavsdk/handle.h>

// MAVSDK 前向声明，避免头文件暴露过多 MAVSDK 细节
namespace mavsdk {
class Mavsdk;
class System;
class Action;
class Telemetry;
class Offboard;
}  // namespace mavsdk

/**
 * @class Px4Controller
 * @brief PX4 仿真控制器
 * @details 通过 MAVSDK 与 PX4 SITL 通信，支持：
 *          - 连接/断开 PX4（UDP 14540 Companion 端口）
 *          - 解锁(arm) / 上锁(disarm)
 *          - 起飞(takeoff) / 降落(land) / 返航(return)
 *          - 悬停(hover) / 位置设置点(offboard position)
 *          - 实时遥测：位置、姿态、速度、电池、飞行状态
 *          所有阻塞调用在工作线程中执行，主线程通过信号接收结果。
 */
class Px4Controller : public QObject
{
    Q_OBJECT

public:
    /// 飞行器连接状态
    enum ConnectionState {
        Disconnected,   ///< 未连接
        Connecting,     ///< 正在连接
        Connected,      ///< 已连接（已发现系统）
        Disconnected_Error  ///< 连接失败
    };
    Q_ENUM(ConnectionState)

    /// 当前飞行模式（与 PX4 主模式映射）
    enum FlightMode {
        Mode_Unknown,       ///< 未知
        Mode_Ground_Idle,   ///< 地面待机（未解锁）
        Mode_Ground_Armed,  ///< 地面已解锁（待起飞）
        Mode_Takeoff,       ///< 起飞中
        Mode_Hold,          ///< 悬停（Hold/Loiter）
        Mode_Mission,       ///< 任务飞行
        Mode_Return,        ///< 返航
        Mode_Land,          ///< 降落中
        Mode_Offboard,      ///< Offboard 模式（外部位置控制）
        Mode_Manual         ///< 手动模式
    };
    Q_ENUM(FlightMode)

    /// 遥测数据快照
    struct TelemetrySnapshot {
        bool   valid;            ///< 数据是否有效
        double latitudeDeg;      ///< 纬度（度）
        double longitudeDeg;     ///< 经度（度）
        double relativeAltM;     ///< 相对起飞点高度（米）
        double absoluteAltM;     ///< 海拔高度（米）
        double speedMS;          ///< 地面速度（m/s）
        double verticalSpeedMS;  ///< 垂直速度（m/s，向上为正）
        double yawDeg;           ///< 航向角（度，0=北，顺时针）
        double rollDeg;          ///< 滚转角（度）
        double pitchDeg;         ///< 俯仰角（度）
        int    batteryPct;       ///< 电池剩余百分比（0-100）
        bool   armed;            ///< 是否已解锁
        FlightMode mode;         ///< 当前飞行模式
        double totalDistKm;      ///< 累计飞行距离（公里）
        double homeDistKm;       ///< 距起飞点距离（公里）
        // 健康状态
        bool   isArmable;        ///< 是否可以解锁
        bool   isLocalPosOk;     ///< 局部位置估计是否可用
        bool   isGlobalPosOk;    ///< 全局位置估计是否可用
        bool   isHomePosOk;      ///< Home 位置是否已设置
        bool   isGyroOk;         ///< 陀螺仪校准是否完成
        bool   isAccelOk;        ///< 加速度计校准是否完成
        bool   isMagOk;          ///< 磁力计校准是否完成
        int    gpsSatellites;    ///< GPS 卫星数
        int    gpsFixType;       ///< GPS 修复类型 (0=NoFix, 2=2D, 3=3D)
    };

    explicit Px4Controller(QObject *parent = nullptr);
    ~Px4Controller();

    // ========== 高层控制接口（线程安全，非阻塞，触发后立即返回） ==========

    /// 连接到 PX4 SITL
    /// @param udpUrl 形如 "udp://:14540"，留空使用默认
    void connectToPx4(const QString &udpUrl = QString());

    /// 断开连接
    void disconnect();

    /// 解锁
    void arm();
    /// 上锁
    void disarm();

    /// 起飞到指定高度
    /// @param altM 目标高度（米），默认 2.5m
    void takeoff(double altM = 2.5);

    /// 解锁并起飞（原子操作，在工作线程中顺序执行 arm → takeoff）
    /// @param altM 目标高度（米），默认 2.5m
    void armAndTakeoff(double altM = 2.5);

    /// 降落
    void land();

    /// 悬停（切换到 Hold 模式）
    void hover();

    /// 返航（Return 模式）
    void returnToLaunch();

    /// 设置目标位置（Offboard 模式，相对起飞点 NED 坐标）
    /// @param northM 北向位移（米，北正南负）
    /// @param eastM  东向位移（米，东正西负）
    /// @param downM  下向位移（米，下正上负，等于 -高度）
    /// @param yawDeg 航向角（度）
    void gotoPosition(double northM, double eastM, double downM, double yawDeg);

    /// 调整飞行高度（保持当前水平位置，通过 Offboard 模式实现）
    /// @details 该方法会读取无人机当前 GPS 坐标，计算 NED 北/东分量，
    ///          然后通过 Offboard 设置新的目标位置（水平不变，高度改变）。
    ///          适用于飞行中（悬停状态）动态调整高度，例如点击高度快捷按钮。
    /// @param altM 目标相对高度（米，正值表示离地高度）
    void setAltitude(double altM);

    /// 设置巡航速度（Offboard 模式下的最大水平速度）
    void setMaxSpeed(double speedMS);

    /// 获取最近一次遥测快照（线程安全）
    TelemetrySnapshot lastTelemetry() const;

    /// 当前连接状态
    ConnectionState connectionState() const;

    /// 设置起飞高度（默认 2.5m）
    void setTakeoffAltitude(double altM);

    /// 设置最大水平速度（影响 Offboard 速度限制）
    void setMaxHorizontalSpeed(double speedMS);

public slots:
    /// 工作线程入口（不要直接调用，由 moveToThread 后启动）
    void start();
    /// 工作线程退出
    void stop();

signals:
    /// 连接状态变化
    void connectionStateChanged(int state);
    /// 遥测数据更新（约 5Hz）
    void telemetryUpdated(const Px4Controller::TelemetrySnapshot &t);
    /// 操作结果反馈（成功/失败 + 描述）
    void operationResult(const QString &operation, bool ok, const QString &msg);
    /// 飞行模式变化
    void flightModeChanged(int mode);

private slots:
    /// 在工作线程中执行连接
    void doConnect();
    /// 系统发现后的初始化（在工作线程中执行，非 MAVSDK 回调线程）
    void onSystemDiscovered();
    /// 在工作线程中执行 arm
    void doArm();
    /// 在工作线程中执行 disarm
    void doDisarm();
    /// 在工作线程中执行起飞
    void doTakeoff();
    /// 在工作线程中执行 arm + takeoff（原子操作）
    void doArmAndTakeoff();
    /// 在工作线程中执行降落
    void doLand();
    /// 在工作线程中执行悬停
    void doHover();
    /// 在工作线程中执行返航
    void doReturn();
    /// 在工作线程中执行位置设置点
    void doGoto();
    /// 在工作线程中执行高度调整（由 setAltitude 通过 invokeMethod 触发）
    /// @details 读取当前遥测快照 → 计算 NED 水平位置 → 设置 Offboard 目标 → 启动 Offboard
    void doSetAltitude();
    /// 在工作线程中轮询遥测数据
    void pollTelemetry();
    /// 连接超时处理
    void onConnectTimeout();

private:
    // 工作线程：所有 MAVSDK 实例在此线程构造与使用
    QThread *m_workerThread;
    // 控制指令参数队列（通过信号投递到工作线程）
    QString m_pendingUrl;
    double  m_pendingTakeoffAlt;
    double  m_pendingNorth;
    double  m_pendingEast;
    double  m_pendingDown;
    double  m_pendingYaw;
    double  m_pendingMaxSpeed;        ///< 待设置的最大水平速度（m/s）
    double  m_pendingAltitude;        ///< 待调整的目标高度（米，由 setAltitude 设置，doSetAltitude 读取）
    bool    m_hasPendingGoto;         ///< 是否有待执行的位置设置点指令

    // MAVSDK 实例（仅在工作线程访问）
    std::shared_ptr<mavsdk::Mavsdk>  m_mavsdk;
    std::shared_ptr<mavsdk::System>  m_system;
    std::shared_ptr<mavsdk::Action>  m_action;
    std::shared_ptr<mavsdk::Telemetry> m_telemetry;
    std::shared_ptr<mavsdk::Offboard>  m_offboard;

    // 订阅句柄（用于取消订阅）
    mavsdk::Handle<> m_newSystemHandle;

    // 连接超时定时器（在工作线程运行）
    QTimer *m_connectTimer;

    // 状态（原子读取，跨线程访问加锁）
    ConnectionState m_state;
    TelemetrySnapshot m_tSnapshot;
    mutable QMutex m_mutex;  // 保护 m_tSnapshot / m_state 跨线程读

    // 遥测轮询定时器（在工作线程运行）
    QTimer *m_pollTimer;

    // 起飞点记录（用于计算距起飞点距离）
    bool   m_hasHome;
    double m_homeLat;
    double m_homeLon;
    double m_lastLat;
    double m_lastLon;
    double m_totalDistM;

    // 配置
    double m_takeoffAltM;        ///< 默认起飞高度
    double m_maxSpeedMS;         ///< 默认最大水平速度

    /// 内部：设置连接状态并发信号
    void setState(ConnectionState s);
    /// 内部：发送操作结果
    void emitResult(const QString &op, bool ok, const QString &msg);
};

#endif  // PX4CONTROLLER_H
