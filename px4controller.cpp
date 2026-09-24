/**
 * @file px4controller.cpp
 * @brief PX4 仿真控制器实现（MAVSDK-C++ v3.17.1）
 */

#include "px4controller.h"

#include <mavsdk/mavsdk.h>
#include <mavsdk/system.h>
#include <mavsdk/plugins/action/action.h>
#include <mavsdk/plugins/telemetry/telemetry.h>
#include <mavsdk/plugins/offboard/offboard.h>

#include <QDebug>
#include <QMetaObject>
#include <QDateTime>
#include <QString>
#include <cmath>
#include <sstream>

// 工具：Haversine 距离（米）
static double haversineM(double lat1, double lon1, double lat2, double lon2)
{
    const double R = 6371000.0;  // 地球半径（米）
    double dLat = (lat2 - lat1) * M_PI / 180.0;
    double dLon = (lon2 - lon1) * M_PI / 180.0;
    double a = std::sin(dLat / 2) * std::sin(dLat / 2) +
               std::cos(lat1 * M_PI / 180.0) * std::cos(lat2 * M_PI / 180.0) *
               std::sin(dLon / 2) * std::sin(dLon / 2);
    double c = 2 * std::atan2(std::sqrt(a), std::sqrt(1 - a));
    return R * c;
}

Px4Controller::Px4Controller(QObject *parent)
    : QObject(nullptr)
    , m_workerThread(new QThread)
    , m_pendingUrl()
    , m_pendingTakeoffAlt(2.5)
    , m_pendingNorth(0.0)
    , m_pendingEast(0.0)
    , m_pendingDown(-2.5)
    , m_pendingYaw(0.0)
    , m_pendingMaxSpeed(5.0)
    , m_hasPendingGoto(false)
    , m_connectTimer(nullptr)
    , m_state(Disconnected)
    , m_mutex()
    , m_pollTimer(nullptr)
    , m_hasHome(false)
    , m_homeLat(0.0)
    , m_homeLon(0.0)
    , m_lastLat(0.0)
    , m_lastLon(0.0)
    , m_totalDistM(0.0)
    , m_takeoffAltM(2.5)
    , m_maxSpeedMS(5.0)
{
    // 初始化遥测快照
    m_tSnapshot.valid = false;
    m_tSnapshot.latitudeDeg = 0.0;
    m_tSnapshot.longitudeDeg = 0.0;
    m_tSnapshot.relativeAltM = 0.0;
    m_tSnapshot.absoluteAltM = 0.0;
    m_tSnapshot.speedMS = 0.0;
    m_tSnapshot.verticalSpeedMS = 0.0;
    m_tSnapshot.yawDeg = 0.0;
    m_tSnapshot.rollDeg = 0.0;
    m_tSnapshot.pitchDeg = 0.0;
    m_tSnapshot.batteryPct = 0;
    m_tSnapshot.armed = false;
    m_tSnapshot.mode = Mode_Unknown;
    m_tSnapshot.totalDistKm = 0.0;
    m_tSnapshot.homeDistKm = 0.0;

    // 把控制器移到工作线程，所有槽函数都在工作线程执行
    this->moveToThread(m_workerThread);

    // 工作线程结束后自动清理
    connect(m_workerThread, &QThread::finished, m_workerThread, &QObject::deleteLater);
}

Px4Controller::~Px4Controller()
{
    stop();
    if (m_workerThread->isRunning()) {
        m_workerThread->quit();
        m_workerThread->wait(3000);
    }
}

// ========== 公共控制接口（主线程调用，通过 invoke 转发到工作线程） ==========

void Px4Controller::connectToPx4(const QString &udpUrl)
{
    // 如果未指定 URL，使用默认 UDP 端口 14540（PX4 SITL 的 Offboard/Companion 端口）
    m_pendingUrl = udpUrl.isEmpty() ? QStringLiteral("udp://:14540") : udpUrl;
    // 将连接任务投递到工作线程执行（MAVSDK 仅在工作线程中使用）
    QMetaObject::invokeMethod(this, "doConnect", Qt::QueuedConnection);
}

void Px4Controller::disconnect()
{
    // 投递清理任务到工作线程（在停止线程前完成所有资源释放）
    QMetaObject::invokeMethod(this, [this]() {
        if (m_pollTimer) {  // 如果遥测轮询定时器存在
            m_pollTimer->stop();  // 停止定时器
            m_pollTimer->deleteLater();  // 标记延迟删除
            m_pollTimer = nullptr;  // 置空指针
        }
        if (m_connectTimer) {  // 如果连接超时定时器存在
            m_connectTimer->stop();  // 停止定时器
            m_connectTimer->deleteLater();  // 标记延迟删除
            m_connectTimer = nullptr;  // 置空指针
        }
        if (m_offboard) {  // 如果 Offboard 插件存在
            m_offboard->stop();  // 停止 Offboard 模式
            m_offboard.reset();  // 释放 Offboard 插件
        }
        m_action.reset();  // 释放 Action 插件
        m_telemetry.reset();  // 释放 Telemetry 插件
        m_system.reset();  // 释放 System 指针
        if (m_mavsdk) {  // 如果 Mavsdk 实例存在
            m_mavsdk.reset();  // 释放 Mavsdk 实例（关闭所有连接）
        }
        m_newSystemHandle = {};  // 清空系统订阅句柄
        setState(Disconnected);  // 更新状态为"已断开"
        emit operationResult("disconnect", true, QStringLiteral("已断开 PX4"));  // 发送断开成功信号
    }, Qt::QueuedConnection);  // 投递到工作线程执行
}

void Px4Controller::arm()
{
    // 将解锁任务投递到工作线程执行
    QMetaObject::invokeMethod(this, "doArm", Qt::QueuedConnection);
}

void Px4Controller::disarm()
{
    // 将上锁任务投递到工作线程执行
    QMetaObject::invokeMethod(this, "doDisarm", Qt::QueuedConnection);
}

void Px4Controller::takeoff(double altM)
{
    // 保存起飞高度（如果有效则使用指定高度，否则使用默认值）
    m_pendingTakeoffAlt = (altM > 0.1 ? altM : m_takeoffAltM);
    // 将起飞任务投递到工作线程执行
    QMetaObject::invokeMethod(this, "doTakeoff", Qt::QueuedConnection);
}

void Px4Controller::armAndTakeoff(double altM)
{
    // 保存起飞高度（如果有效则使用指定高度，否则使用默认值）
    m_pendingTakeoffAlt = (altM > 0.1 ? altM : m_takeoffAltM);
    // 将解锁+起飞任务投递到工作线程执行
    QMetaObject::invokeMethod(this, "doArmAndTakeoff", Qt::QueuedConnection);
}

void Px4Controller::land()
{
    // 将降落任务投递到工作线程执行
    QMetaObject::invokeMethod(this, "doLand", Qt::QueuedConnection);
}

void Px4Controller::hover()
{
    // 将悬停任务投递到工作线程执行
    QMetaObject::invokeMethod(this, "doHover", Qt::QueuedConnection);
}

void Px4Controller::returnToLaunch()
{
    // 将返航任务投递到工作线程执行
    QMetaObject::invokeMethod(this, "doReturn", Qt::QueuedConnection);
}

void Px4Controller::gotoPosition(double northM, double eastM, double downM, double yawDeg)
{
    m_pendingNorth = northM;  // 保存北向位移（米）
    m_pendingEast = eastM;  // 保存东向位移（米）
    m_pendingDown = downM;  // 保存下向位移（米）
    m_pendingYaw = yawDeg;  // 保存航向角（度）
    m_hasPendingGoto = true;  // 标记有待执行的位置设置点指令
    // 将 Offboard 飞行任务投递到工作线程执行
    QMetaObject::invokeMethod(this, "doGoto", Qt::QueuedConnection);
}

/**
 * @brief 调整无人机飞行高度（主线程调用，线程安全）
 * @details 将目标高度保存到 m_pendingAltitude，然后通过 QMetaObject::invokeMethod
 *          将 doSetAltitude 投递到工作线程异步执行。这样避免了主线程直接访问
 *          MAVSDK 实例（MAVSDK 实例仅在工作线程中构造和使用）。
 * @param altM 目标相对高度（米，正值表示离地高度）
 */
void Px4Controller::setAltitude(double altM)
{
    m_pendingAltitude = altM;  // 保存目标高度到成员变量，供工作线程的 doSetAltitude 读取
    // 通过 QMetaObject::invokeMethod 将 doSetAltitude 投递到工作线程异步执行
    // Qt::QueuedConnection 确保在工作线程的事件循环中执行，避免跨线程访问 MAVSDK 实例
    QMetaObject::invokeMethod(this, "doSetAltitude", Qt::QueuedConnection);
}

void Px4Controller::setMaxSpeed(double speedMS)
{
    m_pendingMaxSpeed = speedMS;  // 保存目标速度到成员变量
    // 通过 QMetaObject::invokeMethod 将 lambda 投递到工作线程异步执行
    QMetaObject::invokeMethod(this, [this]() {
        if (!m_action) return;  // Action 插件未初始化，直接返回
        auto r = m_action->set_current_speed(m_pendingMaxSpeed);  // 设置当前最大速度
        emitResult("setMaxSpeed", r == mavsdk::Action::Result::Success,  // 判断是否设置成功
                   QStringLiteral("最大速度=") + QString::number(m_pendingMaxSpeed, 'f', 2));  // 发送结果信号
    }, Qt::QueuedConnection);  // 使用队列连接确保在工作线程中执行
}

Px4Controller::TelemetrySnapshot Px4Controller::lastTelemetry() const
{
    QMutexLocker lk(&m_mutex);  // 加锁保护 m_tSnapshot 的并发访问
    return m_tSnapshot;  // 返回遥测快照副本
}

Px4Controller::ConnectionState Px4Controller::connectionState() const
{
    QMutexLocker lk(&m_mutex);  // 加锁保护 m_state 的并发访问
    return m_state;  // 返回当前连接状态副本
}

void Px4Controller::setTakeoffAltitude(double altM)
{
    m_takeoffAltM = altM;  // 保存起飞高度到成员变量
    QMetaObject::invokeMethod(this, [this]() {
        if (!m_action) return;  // Action 插件未初始化，直接返回
        m_action->set_takeoff_altitude(static_cast<float>(m_takeoffAltM));  // 设置起飞高度（浮点精度）
    }, Qt::QueuedConnection);  // 投递到工作线程执行
}

void Px4Controller::setMaxHorizontalSpeed(double speedMS)
{
    m_maxSpeedMS = speedMS;  // 保存最大水平速度到成员变量
    setMaxSpeed(speedMS);  // 调用 setMaxSpeed 实际发送指令
}

// ========== 工作线程入口/退出 ==========

void Px4Controller::start()
{
    if (!m_workerThread->isRunning()) {  // 检查工作线程是否已在运行
        m_workerThread->start();  // 启动工作线程（事件循环开始运行）
    }
}

void Px4Controller::stop()
{
    // 投递清理任务到工作线程（在停止线程前完成）
    QMetaObject::invokeMethod(this, [this]() {
        if (m_pollTimer) {  // 如果遥测轮询定时器存在
            m_pollTimer->stop();  // 停止定时器
            m_pollTimer->deleteLater();  // 标记延迟删除
            m_pollTimer = nullptr;  // 置空指针
        }
        if (m_connectTimer) {  // 如果连接超时定时器存在
            m_connectTimer->stop();  // 停止定时器
            m_connectTimer->deleteLater();  // 标记延迟删除
            m_connectTimer = nullptr;  // 置空指针
        }
    }, Qt::QueuedConnection);  // 投递到工作线程执行
}

// ========== 工作线程槽实现 ==========

void Px4Controller::doConnect()
{
    qDebug() << "[Px4Controller] doConnect url=" << m_pendingUrl;  // 调试日志：打印连接地址
    setState(Connecting);  // 更新状态为"连接中"，发送状态变更信号

    // 清理之前的连接（如果有）
    if (m_pollTimer) {  // 如果遥测定时器存在
        m_pollTimer->stop();  // 停止定时器
        m_pollTimer->deleteLater();  // 标记延迟删除
        m_pollTimer = nullptr;  // 置空指针
    }
    if (m_connectTimer) {  // 如果连接超时定时器存在
        m_connectTimer->stop();  // 停止定时器
        m_connectTimer->deleteLater();  // 标记延迟删除
        m_connectTimer = nullptr;  // 置空指针
    }
    if (m_newSystemHandle.valid()) {  // 如果旧系统订阅句柄有效
        m_newSystemHandle = {};  // 清空句柄（取消订阅）
    }
    m_action.reset();  // 释放 Action 插件
    m_telemetry.reset();  // 释放 Telemetry 插件
    m_offboard.reset();  // 释放 Offboard 插件
    m_system.reset();  // 释放 System 指针
    if (m_mavsdk) {  // 如果 Mavsdk 实例存在
        m_mavsdk.reset();  // 释放 Mavsdk 实例
    }

    try {
        // 配置 Mavsdk 组件类型为 GroundStation（地面站），PX4 会信任此组件的指令
        mavsdk::Mavsdk::Configuration cfg(mavsdk::ComponentType::GroundStation);
        m_mavsdk = std::make_shared<mavsdk::Mavsdk>(cfg);  // 创建 Mavsdk 实例
    } catch (const std::exception &e) {
        emitResult("connect", false, QStringLiteral("Mavsdk 构造异常:") + e.what());  // 发送错误信号
        setState(Disconnected_Error);  // 更新状态为"连接失败"
        return;
    }

    // 建立 UDP 连接到 PX4 SITL
    /**
     * @brief connResult
     * @brief add_any_connection 通用连接入口，支持自动识别传入的URL协议类型，自动适配UDP、TCP、串口等多种通信方式，无需单独调用不同协议的连接函数
     */
    auto connResult = m_mavsdk->add_any_connection(m_pendingUrl.toStdString());  // 连接到指定 URL
    if (connResult != mavsdk::ConnectionResult::Success) {  // 检查连接结果
        std::ostringstream oss;  // 用于将枚举转换为字符串
        oss << connResult;  // 将连接结果枚举写入流
        emitResult("connect", false,  // 发送连接失败信号
                   QStringLiteral("连接失败: ") + QString::fromStdString(oss.str()));  // 附带错误描述
        setState(Disconnected_Error);  // 更新状态为"连接失败"
        return;
    }
    qDebug() << "[Px4Controller] UDP 连接已建立，等待 PX4 系统发现...";  // 调试日志

    // 启动连接超时定时器（10秒），防止无限等待
    m_connectTimer = new QTimer(this);  // 创建定时器
    m_connectTimer->setSingleShot(true);  // 设置为单次触发
    m_connectTimer->setInterval(10000);  // 设置超时时间为 10000ms（10秒）
    connect(m_connectTimer, &QTimer::timeout, this, &Px4Controller::onConnectTimeout);  // 连接超时信号到处理槽
    m_connectTimer->start();  // 启动定时器

    // 异步等待系统 —— 回调在 MAVSDK 内部线程执行，只做最少量工作
    m_newSystemHandle = m_mavsdk->subscribe_on_new_system([this]() {
        // 此回调在 MAVSDK 内部线程执行，不能做 Qt 操作或阻塞调用
        // 仅保存 system 指针，然后将实际初始化工作投递到工作线程
        auto systems = m_mavsdk->systems();  // 获取当前所有已发现的系统
        if (systems.empty()) return;  // 如果没有系统，直接返回

        // 保存 system 指针（shared_ptr 线程安全引用计数）
        m_system = systems.back();  // 取最后一个系统（通常只有一个）

        // 用 invokeMethod 将初始化工作投递到工作线程
        QMetaObject::invokeMethod(this, "onSystemDiscovered", Qt::QueuedConnection);  // 投递到工作线程
    });
}

void Px4Controller::onSystemDiscovered()
{
    // 此函数在工作线程执行（由 MAVSDK 回调通过 invokeMethod 投递过来）
    if (!m_system) {  // 检查 system 指针是否有效
        qWarning() << "[Px4Controller] onSystemDiscovered: m_system 为空";  // 输出警告日志
        setState(Disconnected_Error);  // 更新状态为"连接失败"
        emitResult("connect", false, QStringLiteral("系统发现失败：无有效 system"));  // 发送失败信号
        return;
    }

    // 停止连接超时定时器（系统已发现，不再需要超时检测）
    if (m_connectTimer) {  // 如果定时器存在
        m_connectTimer->stop();  // 停止定时器
        m_connectTimer->deleteLater();  // 标记延迟删除
        m_connectTimer = nullptr;  // 置空指针
    }

    qDebug() << "[Px4Controller] PX4 系统已发现，初始化插件...";  // 调试日志

    // 创建 MAVSDK 插件（每个插件封装了与 PX4 的一类交互功能）
    m_action    = std::make_shared<mavsdk::Action>(m_system);  // Action 插件：起飞、降落、解锁等
    m_telemetry = std::make_shared<mavsdk::Telemetry>(m_system);  // Telemetry 插件：遥测数据订阅
    m_offboard  = std::make_shared<mavsdk::Offboard>(m_system);  // Offboard 插件：外部位置控制

    // 配置起飞高度与速度（在插件创建后立即设置默认值）
    m_action->set_takeoff_altitude(static_cast<float>(m_takeoffAltM));  // 设置默认起飞高度
    m_action->set_current_speed(m_maxSpeedMS);  // 设置默认最大水平速度

    // 启动遥测轮询定时器（在工作线程中运行，使用 DirectConnection）
    if (!m_pollTimer) {  // 如果定时器尚未创建
        m_pollTimer = new QTimer(this);  // 创建定时器（父对象为 this，析构时自动清理）
        m_pollTimer->setInterval(200);  // 设置轮询间隔为 200ms（5Hz，每秒5次）
        // 连接定时器超时信号到 pollTelemetry 槽，使用 DirectConnection 直接在工作线程执行
        connect(m_pollTimer, &QTimer::timeout, this, &Px4Controller::pollTelemetry,
                Qt::DirectConnection);
    }
    m_pollTimer->start();  // 启动遥测轮询

    qDebug() << "[Px4Controller] 系统已连接！开始遥测";  // 调试日志
    setState(Connected);  // 更新状态为"已连接"
    emit operationResult("connect", true, QStringLiteral("已连接 PX4 系统"));  // 发送连接成功信号
}

void Px4Controller::onConnectTimeout()
{
    qWarning() << "[Px4Controller] 连接超时：10秒内未发现 PX4 系统";  // 输出警告日志
    if (m_connectTimer) {  // 如果连接超时定时器存在
        m_connectTimer->stop();  // 停止定时器
        m_connectTimer->deleteLater();  // 标记延迟删除
        m_connectTimer = nullptr;  // 置空指针
    }
    // 清理 MAVSDK 资源（释放与 PX4 的连接）
    if (m_newSystemHandle.valid()) {  // 如果系统订阅句柄有效
        m_newSystemHandle = {};  // 清空句柄（取消订阅新系统通知）
    }
    if (m_mavsdk) {  // 如果 Mavsdk 实例存在
        m_mavsdk.reset();  // 释放 Mavsdk 实例（关闭所有连接）
    }
    setState(Disconnected_Error);  // 更新状态为"连接失败"
    emitResult("connect", false,  // 发送连接超时失败信号
               QStringLiteral("连接超时：10秒内未发现 PX4 系统，请确认 SITL 已启动"));  // 附带提示信息
}

void Px4Controller::doArm()
{
    if (!m_action) { emitResult("arm", false, QStringLiteral("未连接")); return; }  // Action 插件未初始化，直接返回

    qDebug() << "[Px4Controller] 尝试解锁...";  // 调试日志
    auto r = m_action->arm();  // 发送解锁指令（blocking call，等待 PX4 响应）
    std::ostringstream oss;  // 用于将枚举转换为字符串
    oss << r;  // 将解锁结果枚举写入流
    QString resultStr = QString::fromStdString(oss.str());  // 转换为 QString
    bool ok = (r == mavsdk::Action::Result::Success);  // 判断是否解锁成功
    qDebug() << "[Px4Controller] arm 结果:" << resultStr;  // 调试日志：打印解锁结果
    emitResult("arm", ok, ok ? QStringLiteral("解锁成功")  // 发送成功信号
                             : QStringLiteral("解锁失败: %1").arg(resultStr));  // 发送失败信号（附带原因）
}

void Px4Controller::doDisarm()
{
    if (!m_action) { emitResult("disarm", false, QStringLiteral("未连接")); return; }  // Action 插件未初始化，直接返回
    auto r = m_action->disarm();  // 发送上锁指令（blocking call）
    std::ostringstream oss;  // 用于将枚举转换为字符串
    oss << r;  // 将上锁结果枚举写入流
    bool ok = (r == mavsdk::Action::Result::Success);  // 判断是否上锁成功
    emitResult("disarm", ok, ok ? QStringLiteral("上锁成功")  // 发送成功信号
                               : QStringLiteral("上锁失败: %1").arg(QString::fromStdString(oss.str())));  // 发送失败信号
}

void Px4Controller::doTakeoff()
{
    if (!m_action) { emitResult("takeoff", false, QStringLiteral("未连接")); return; }  // Action 插件未初始化，直接返回

    // 先设置起飞高度，并检查结果
    auto altResult = m_action->set_takeoff_altitude(static_cast<float>(m_pendingTakeoffAlt));  // 设置起飞高度
    if (altResult != mavsdk::Action::Result::Success) {  // 如果设置失败
        std::ostringstream oss;  // 用于将枚举转换为字符串
        oss << altResult;  // 将错误码写入流
        qWarning() << "[Px4Controller] set_takeoff_altitude 失败:"  // 输出警告日志
                   << QString::fromStdString(oss.str());  // 附带错误描述
    }

    // 设置速度
    if (m_pendingMaxSpeed > 0) {  // 如果设置了最大速度
        m_action->set_current_speed(m_pendingMaxSpeed);  // 发送最大速度设置指令
    }

    qDebug() << "[Px4Controller] 尝试起飞 (高度=" << m_pendingTakeoffAlt << "m)...";  // 调试日志
    auto r = m_action->takeoff();  // 发送起飞指令（blocking call，PX4 会自动起飞到指定高度）
    std::ostringstream oss;  // 用于将枚举转换为字符串
    oss << r;  // 将起飞结果枚举写入流
    QString resultStr = QString::fromStdString(oss.str());  // 转换为 QString
    bool ok = (r == mavsdk::Action::Result::Success);  // 判断是否起飞成功
    qDebug() << "[Px4Controller] takeoff 结果:" << resultStr;  // 调试日志：打印起飞结果
    emitResult("takeoff", ok, ok ? QStringLiteral("起飞成功")  // 发送成功信号
                                : QStringLiteral("起飞失败: %1").arg(resultStr));  // 发送失败信号（附带原因）
}

void Px4Controller::doArmAndTakeoff()
{
    if (!m_action) { emitResult("armAndTakeoff", false, QStringLiteral("未连接")); return; }  // Action 插件未初始化，直接返回

    qDebug() << "[Px4Controller] ===== 开始 armAndTakeoff 流程 =====";  // 调试日志：流程开始

    // 0. 检查无人机是否已在空中（避免重复起飞）
    if (m_telemetry) {  // Telemetry 插件存在
        try {
            auto vcl = m_telemetry->landed_state();  // 获取降落状态
            if (vcl == mavsdk::Telemetry::LandedState::InAir) {  // 如果已在空中
                emitResult("armAndTakeoff", false,  // 发送失败信号
                           QStringLiteral("无人机已在空中，无法再次起飞"));  // 附带说明
                return;
            }
        } catch (...) {}  // 捕获异常（遥测可能尚未就绪）
    }

    // 1. 先设置起飞高度（必须在起飞前设置）
    auto altResult = m_action->set_takeoff_altitude(static_cast<float>(m_pendingTakeoffAlt));  // 设置起飞高度
    if (altResult != mavsdk::Action::Result::Success) {  // 如果设置失败
        std::ostringstream oss;  // 用于将枚举转换为字符串
        oss << altResult;  // 将错误码写入流
        qWarning() << "[Px4Controller] set_takeoff_altitude 失败:"  // 输出警告日志
                   << QString::fromStdString(oss.str());  // 附带错误描述
    }

    // 2. 检查是否已解锁（避免重复解锁）
    bool alreadyArmed = false;  // 初始化已解锁标志为 false
    if (m_telemetry) {  // Telemetry 插件存在
        try { alreadyArmed = m_telemetry->armed(); } catch (...) {}  // 读取解锁状态（异常则保持 false）
    }

    // 3. 解锁（如果尚未解锁）
    bool armOk = alreadyArmed;  // 初始化解锁结果为当前已解锁状态
    if (!alreadyArmed) {  // 如果尚未解锁
        qDebug() << "[Px4Controller] 尝试解锁...";  // 调试日志
        auto armResult = m_action->arm();  // 发送解锁指令（blocking call）
        std::ostringstream armOss;  // 用于将枚举转换为字符串
        armOss << armResult;  // 将解锁结果枚举写入流
        QString armStr = QString::fromStdString(armOss.str());  // 转换为 QString
        armOk = (armResult == mavsdk::Action::Result::Success);  // 判断是否解锁成功
        qDebug() << "[Px4Controller] arm 结果:" << armStr;  // 调试日志：打印解锁结果
    }

    if (!armOk) {  // 如果解锁失败
        emitResult("armAndTakeoff", false, QStringLiteral("解锁失败"));  // 发送失败信号
        return;
    }

    if (!alreadyArmed) {  // 如果本次刚解锁成功（之前未解锁）
        emit operationResult("arm", true, QStringLiteral("解锁成功"));  // 单独发送解锁成功信号
    }

    // 4. 解锁成功，执行起飞
    qDebug() << "[Px4Controller] 解锁成功，尝试起飞 (高度=" << m_pendingTakeoffAlt << "m)...";  // 调试日志
    auto takeoffResult = m_action->takeoff();  // 发送起飞指令（blocking call，PX4 自动起飞）
    std::ostringstream takeoffOss;  // 用于将枚举转换为字符串
    takeoffOss << takeoffResult;  // 将起飞结果枚举写入流
    QString takeoffStr = QString::fromStdString(takeoffOss.str());  // 转换为 QString
    bool takeoffOk = (takeoffResult == mavsdk::Action::Result::Success);  // 判断是否起飞成功
    qDebug() << "[Px4Controller] takeoff 结果:" << takeoffStr;  // 调试日志：打印起飞结果

    emitResult("armAndTakeoff", takeoffOk,  // 发送合并结果信号
               takeoffOk ? QStringLiteral("解锁并起飞成功")  // 成功消息
                         : QStringLiteral("起飞失败: %1").arg(takeoffStr));  // 失败消息（附带原因）
}

void Px4Controller::doLand()
{
    if (!m_action) { emitResult("land", false, QStringLiteral("未连接")); return; }  // Action 插件未初始化，直接返回
    if (m_offboard) {  // 如果 Offboard 插件存在
        m_offboard->stop();  // 停止 Offboard 模式（降落前先退出外部控制）
    }
    auto r = m_action->land();  // 发送降落指令（blocking call，PX4 自动降落）
    emitResult("land", r == mavsdk::Action::Result::Success,  // 判断是否降落成功
               QStringLiteral("land 完成"));  // 发送结果信号
}

void Px4Controller::doHover()
{
    if (!m_action) { emitResult("hover", false, QStringLiteral("未连接")); return; }  // Action 插件未初始化，直接返回
    if (m_offboard) {  // 如果 Offboard 插件存在
        m_offboard->stop();  // 停止 Offboard 模式（悬停前先退出外部控制）
    }
    auto r = m_action->hold();  // 发送悬停指令（blocking call，PX4 保持当前位置悬停）
    emitResult("hover", r == mavsdk::Action::Result::Success,  // 判断是否悬停指令发送成功
               QStringLiteral("hold 完成"));  // 发送结果信号
}

void Px4Controller::doReturn()
{
    if (!m_action) { emitResult("return", false, QStringLiteral("未连接")); return; }  // Action 插件未初始化，直接返回
    if (m_offboard) {  // 如果 Offboard 插件存在
        m_offboard->stop();  // 停止 Offboard 模式（返航前先退出外部控制）
    }
    auto r = m_action->return_to_launch();  // 发送返航指令（blocking call，PX4 返回起飞点）
    emitResult("return", r == mavsdk::Action::Result::Success,  // 判断是否返航指令发送成功
               QStringLiteral("RTL 完成"));  // 发送结果信号（RTL = Return To Launch）
}

void Px4Controller::doGoto()
{
    if (!m_offboard || !m_system) {  // 检查 Offboard 插件和 System 是否可用
        emitResult("goto", false, QStringLiteral("未连接"));  // 发送失败信号
        m_hasPendingGoto = false;  // 清除待执行标志
        return;
    }
    mavsdk::Offboard::PositionNedYaw target;  // 构造 Offboard 目标位置结构体
    target.north_m = static_cast<float>(m_pendingNorth);  // 设置北向位移（米）
    target.east_m  = static_cast<float>(m_pendingEast);  // 设置东向位移（米）
    target.down_m  = static_cast<float>(m_pendingDown);  // 设置下向位移（米，NED 中 down 为正）
    target.yaw_deg = static_cast<float>(m_pendingYaw);  // 设置航向角（度）
    m_offboard->set_position_ned(target);  // 设置 Offboard 目标位置（必须在 start 之前调用）

    auto r = m_offboard->start();  // 启动 Offboard 模式（PX4 将飞行到目标位置）
    emitResult("goto", r == mavsdk::Offboard::Result::Success,  // 判断是否启动成功
               QStringLiteral("offboard goto (N=%1 E=%2 D=%3 yaw=%4)")  // 发送结果信号
                   .arg(m_pendingNorth, 0, 'f', 1)  // 格式化北向位移
                   .arg(m_pendingEast, 0, 'f', 1)  // 格式化东向位移
                   .arg(m_pendingDown, 0, 'f', 1)  // 格式化下向位移
                   .arg(m_pendingYaw, 0, 'f', 1));  // 格式化航向角
    m_hasPendingGoto = false;  // 清除待执行标志
}

/**
 * @brief 在工作线程中执行高度调整
 * @details 通过 Offboard 模式实现高度调整，核心流程：
 *          1. 检查 Offboard 和 System 是否可用
 *          2. 读取最新遥测快照（包含 GPS 坐标、航向角等）
 *          3. 根据 GPS 坐标计算当前 NED 北/东分量（相对起飞点）
 *          4. 构造 Offboard 目标：水平位置保持不变，高度改为 m_pendingAltitude
 *          5. 设置 Offboard 位置设置点并启动 Offboard 模式
 *          6. 发送操作结果信号
 *
 * NED 坐标系说明：
 *   - North/East/Down 为正，高度 = -Down
 *   - down_m = -altM（高度 1.5m 对应 down = -1.5）
 */
void Px4Controller::doSetAltitude()
{
    // 检查 Offboard 插件和 System 是否已初始化（未连接则无法执行）
    if (!m_offboard || !m_system) {
        emitResult("setAltitude", false, QStringLiteral("未连接"));  // 发送失败信号
        return;  // 直接返回，不执行后续逻辑
    }

    // 读取当前遥测快照（线程安全，通过 m_mutex 保护内部 m_tSnapshot）
    TelemetrySnapshot snap = lastTelemetry();
    if (!snap.valid) {  // 遥测数据无效（可能尚未收到任何遥测）
        emitResult("setAltitude", false, QStringLiteral("遥测数据无效"));  // 发送失败信号
        return;  // 直接返回
    }

    // ===== 计算当前 NED 水平位置（相对起飞点 home） =====
    double lat = snap.latitudeDeg;   // 当前纬度（度）
    double lon = snap.longitudeDeg;  // 当前经度（度）
    double north = 0.0;              // 北向位移（米），初始化为 0
    double east = 0.0;               // 东向位移（米），初始化为 0

    // 如果已有 home 点且 GPS 坐标有效，通过经纬度差计算 NED 分量
    if (m_hasHome && std::isfinite(lat) && std::isfinite(lon) &&
        (lat != 0.0 || lon != 0.0)) {
        // 简单线性近似：1度纬度 ≈ 111320m（地球周长约40075km / 360度）
        north = (lat - m_homeLat) * 111320.0;  // 纬度差 × 每度米数 = 北向位移
        // 经度方向需要乘以 cos(纬度) 修正（高纬度地区经度间距缩短）
        double cosLat = std::cos(lat * M_PI / 180.0);  // 将纬度转换为弧度并取余弦
        east = (lon - m_homeLon) * 111320.0 * cosLat;  // 经度差 × 每度米数 × 纬度修正 = 东向位移
    }

    // 如果 GPS 不可用（坐标无效或未初始化），使用上一次的位置设置点作为回退
    if (!std::isfinite(north) || !std::isfinite(east)) {
        north = m_pendingNorth;  // 回退到上次设置的北向位置
        east = m_pendingEast;    // 回退到上次设置的东向位置
    }

    // ===== 构造 Offboard 目标位置 =====
    // 保持当前水平位置（north/east 不变），仅改变高度（down_m）
    mavsdk::Offboard::PositionNedYaw target;                        // Offboard 目标位置结构体
    target.north_m = static_cast<float>(north);                     // 北向：保持当前位置
    target.east_m  = static_cast<float>(east);                      // 东向：保持当前位置
    target.down_m  = static_cast<float>(-m_pendingAltitude);        // 下向：NED 中 down 为正，高度为负（如高度1.5m → down=-1.5）
    target.yaw_deg = static_cast<float>(snap.yawDeg);               // 航向角：保持当前航向不变

    // 调试输出：打印目标高度和 NED 坐标
    qDebug() << "[Px4Controller] setAltitude: target alt=" << m_pendingAltitude
             << "m (N=" << north << ", E=" << east << ", D=" << -m_pendingAltitude << ")";

    // 设置 Offboard 位置设置点（必须在 start 之前调用，否则 start 会失败）
    m_offboard->set_position_ned(target);

    // 启动 Offboard 模式，PX4 将飞行到目标位置
    auto r = m_offboard->start();                                   // 返回 Offboard::Result 枚举
    bool ok = (r == mavsdk::Offboard::Result::Success);            // 判断是否成功启动

    // 发送操作结果信号（成功/失败 + 描述信息），主线程接收后可更新界面或弹窗提示
    emitResult("setAltitude", ok,
               ok ? QStringLiteral("高度调整到 %1m 指令已发送").arg(m_pendingAltitude, 0, 'f', 2)  // 成功消息
                  : QStringLiteral("高度调整失败: %1").arg(static_cast<int>(r)));                   // 失败消息（附带错误码）
}

void Px4Controller::pollTelemetry()
{
    if (!m_telemetry || !m_system) return;

    TelemetrySnapshot t;
    t.valid = false;

    // 位置
    auto pos = m_telemetry->position();
    t.latitudeDeg   = pos.latitude_deg;
    t.longitudeDeg  = pos.longitude_deg;
    t.relativeAltM  = static_cast<double>(pos.relative_altitude_m);
    t.absoluteAltM  = static_cast<double>(pos.absolute_altitude_m);

    // 速度（NED）
    auto vel = m_telemetry->velocity_ned();
    double vx = static_cast<double>(vel.north_m_s);
    double vy = static_cast<double>(vel.east_m_s);
    double vz = static_cast<double>(vel.down_m_s);
    t.speedMS = std::sqrt(vx * vx + vy * vy);
    t.verticalSpeedMS = -vz;  // 向上为正

    // 姿态（欧拉角）
    auto att = m_telemetry->attitude_euler();
    t.yawDeg   = static_cast<double>(att.yaw_deg);
    t.rollDeg  = static_cast<double>(att.roll_deg);
    t.pitchDeg = static_cast<double>(att.pitch_deg);

    // 电池
    auto bat = m_telemetry->battery();
    t.batteryPct = static_cast<int>(bat.remaining_percent);

    // 解锁状态
    t.armed = m_telemetry->armed();

    // 健康状态
    try {
        auto health = m_telemetry->health();
        t.isArmable     = health.is_armable;
        t.isLocalPosOk  = health.is_local_position_ok;
        t.isGlobalPosOk = health.is_global_position_ok;
        t.isHomePosOk   = health.is_home_position_ok;
        t.isGyroOk      = health.is_gyrometer_calibration_ok;
        t.isAccelOk     = health.is_accelerometer_calibration_ok;
        t.isMagOk       = health.is_magnetometer_calibration_ok;
    } catch (...) {
        t.isArmable = false;
        t.isLocalPosOk = false;
        t.isGlobalPosOk = false;
        t.isHomePosOk = false;
        t.isGyroOk = false;
        t.isAccelOk = false;
        t.isMagOk = false;
    }

    // GPS 信息
    try {
        auto gpsInfo = m_telemetry->gps_info();
        t.gpsSatellites = gpsInfo.num_satellites;
        t.gpsFixType = static_cast<int>(gpsInfo.fix_type);
    } catch (...) {
        t.gpsSatellites = 0;
        t.gpsFixType = 0;
    }

    // 飞行模式映射（MAVSDK v3 FlightMode 枚举）
    auto fm = m_telemetry->flight_mode();
    switch (fm) {
    case mavsdk::Telemetry::FlightMode::Ready:          t.mode = Mode_Ground_Armed; break;
    case mavsdk::Telemetry::FlightMode::Takeoff:        t.mode = Mode_Takeoff; break;
    case mavsdk::Telemetry::FlightMode::Hold:           t.mode = Mode_Hold; break;
    case mavsdk::Telemetry::FlightMode::Mission:        t.mode = Mode_Mission; break;
    case mavsdk::Telemetry::FlightMode::ReturnToLaunch: t.mode = Mode_Return; break;
    case mavsdk::Telemetry::FlightMode::Land:           t.mode = Mode_Land; break;
    case mavsdk::Telemetry::FlightMode::Offboard:       t.mode = Mode_Offboard; break;
    case mavsdk::Telemetry::FlightMode::Manual:         t.mode = Mode_Manual; break;
    default:
        if (t.armed) t.mode = Mode_Ground_Armed;
        else         t.mode = Mode_Ground_Idle;
        break;
    }

    // 基础遥测数据始终有效（高度、速度、姿态、电池等不依赖 GPS）
    // 注意：之前 t.valid = true 放在下面的 GPS 判断内，导致 GPS 未定位时
    //       所有遥测数据被标记为无效，界面高度等数据无法更新。
    //       修复后将 t.valid = true 移到此处，因为高度等数据不依赖 GPS。
    t.valid = true;  // 标记遥测快照为有效（无论 GPS 是否可用）

    // 累计飞行距离 + 距起飞点距离（仅在有有效 GPS 时计算）
    if (std::isfinite(t.latitudeDeg) && std::isfinite(t.longitudeDeg) &&  // 检查经纬度是否为有效数值
        (t.latitudeDeg != 0.0 || t.longitudeDeg != 0.0)) {                // 排除 (0,0) 坐标（GPS 未定位时的默认值）
        if (!m_hasHome) {
            // 首次获得有效 GPS，记录起飞点（home）
            m_homeLat = t.latitudeDeg;
            m_homeLon = t.longitudeDeg;
            m_lastLat = t.latitudeDeg;
            m_lastLon = t.longitudeDeg;
            m_hasHome = true;
            m_totalDistM = 0.0;
        } else {
            // 计算与上一次位置的位移，累加到总飞行距离
            double seg = haversineM(m_lastLat, m_lastLon, t.latitudeDeg, t.longitudeDeg);
            if (seg < 1000.0) {  // 过滤异常跳变（>1000m 视为 GPS 异常）
                m_totalDistM += seg;
            }
            m_lastLat = t.latitudeDeg;
            m_lastLon = t.longitudeDeg;
        }
        t.totalDistKm = m_totalDistM / 1000.0;
        t.homeDistKm  = haversineM(m_homeLat, m_homeLon, t.latitudeDeg, t.longitudeDeg) / 1000.0;
    }

    // 写入共享快照
    {
        QMutexLocker lk(&m_mutex);
        m_tSnapshot = t;
    }
    emit telemetryUpdated(t);

    // 飞行模式变化信号
    static FlightMode lastFm = Mode_Unknown;
    if (t.mode != lastFm) {
        lastFm = t.mode;
        emit flightModeChanged(static_cast<int>(t.mode));
    }
}

// ========== 内部辅助 ==========

void Px4Controller::setState(ConnectionState s)
{
    {
        QMutexLocker lk(&m_mutex);
        m_state = s;
    }
    emit connectionStateChanged(static_cast<int>(s));
}

void Px4Controller::emitResult(const QString &op, bool ok, const QString &msg)
{
    qDebug().noquote() << "[Px4Controller]" << op << (ok ? "OK" : "FAIL") << msg;
    emit operationResult(op, ok, msg);
}
