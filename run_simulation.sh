#!/usr/bin/env bash
# =============================================================================
# PX4 SITL + Gazebo Classic + QGC 一键启动脚本
# 用法:
#   ./run_simulation.sh              # 启动全部（PX4+Gazebo+QGC）
#   ./run_simulation.sh px4          # 仅启动 PX4 SITL + Gazebo
#   ./run_simulation.sh qgc          # 仅启动 QGC
#   ./run_simulation.sh stop         # 停止全部
#
# 端口约定:
#   PX4 SITL  -> 14540 (offboard/companion)  Qt 程序连接
#   PX4 SITL  -> 14550 (GCS)                 QGC 连接
# =============================================================================
set -u

# --------------------------- 路径配置 ---------------------------
PX4_ROOT="${PX4_ROOT:-$HOME/PX4-Autopilot}"
PX4_BIN="$PX4_ROOT/build/px4_sitl_default/bin/px4"
PX4_RC="$PX4_ROOT/build/px4_sitl_default/etc/init.d-posix/rcS"
QGC_APP="${QGC_APP:-$HOME/QGroundControl.AppImage}"
WORKDIR="$HOME/px4_sim_logs"

AIRFRAME="${AIRFRAME:-gazebo-classic_iris}"   # 默认 iris 四旋翼

# --------------------------- 颜色 ---------------------------
R='\033[0;31m'; G='\033[0;32m'; Y='\033[1;33m'; C='\033[0;36m'; N='\033[0m'
log()  { echo -e "${G}[SIM]${N} $*"; }
warn() { echo -e "${Y}[WARN]${N} $*"; }
err()  { echo -e "${R}[ERR]${N} $*" >&2; }

# --------------------------- 前置检查 ---------------------------
check_env() {
    [ -x "$PX4_BIN" ] || { err "找不到 PX4 可执行文件: $PX4_BIN"; err "请先在 $PX4_ROOT 执行: make px4_sitl gazebo-classic"; exit 1; }
    [ -f "$PX4_RC" ]  || { err "找不到 PX4 启动脚本: $PX4_RC"; exit 1; }
    command -v gazebo >/dev/null 2>&1 || { err "未安装 gazebo (classic)，请: sudo apt install gazebo11 libgazebo11-dev"; exit 1; }
    command -v roscore >/dev/null 2>&1 && warn "检测到 ROS，注意 PX4 Gazebo 不强制依赖 ROS。"
}

# --------------------------- 启动 PX4 + Gazebo ---------------------------
start_px4() {
    check_env
    mkdir -p "$WORKDIR"
    cd "$WORKDIR"

    # 若已运行则提示
    if pgrep -f "px4 .*init.d-posix" >/dev/null 2>&1; then
        warn "PX4 SITL 已在运行，先停止再启动。运行: $0 stop"
        return 1
    fi

    log "启动 PX4 SITL + Gazebo Classic (airframe=$AIRFRAME)..."
    log "  PX4_BIN  = $PX4_BIN"
    log "  日志目录 = $WORKDIR"
    log "  Companion 端口 = udp://:14540 (Qt 程序连这个)"
    log "  GCS       端口 = udp://:14550 (QGC 连这个)"

    # 设置 Gazebo 模型/插件路径
    export PX4_HOME_LAT="${PX4_HOME_LAT:-30.5728}"
    export PX4_HOME_LON="${PX4_HOME_LON:-104.0668}"
    export PX4_HOME_ALT="${PX4_HOME_ALT:-508.0}"
    export PX4_SIM_MODEL="$AIRFRAME"

    export GAZEBO_MODEL_PATH="${GAZEBO_MODEL_PATH:-}:$PX4_ROOT/Tools/simulation/gazebo-classic/sitl_gazebo-classic/models"
    export GAZEBO_PLUGIN_PATH="${GAZEBO_PLUGIN_PATH:-}:$PX4_ROOT/build/px4_sitl_default/build_gazebo-classic"

    # 启动 PX4 + Gazebo（后台）
    nohup "$PX4_BIN" -d "$PX4_ROOT/build/px4_sitl_default/etc" -s "etc/init.d-posix/rcS" 0 > "$WORKDIR/px4.out" 2>&1 &
    local px4pid=$!
    echo "$px4pid" > "$WORKDIR/px4.pid"
    log "PX4 已启动 (PID=$px4pid)，日志: $WORKDIR/px4.out"

    # 等待 SITL 就绪
    log "等待 PX4 SITL 就绪 (最多 30s)..."
    for i in $(seq 1 30); do
        if grep -q "Ready for takeoff\|onboarding\| commander started\|Starting communication" "$WORKDIR/px4.out" 2>/dev/null; then
            log "PX4 SITL 就绪 (用时 ${i}s)"
            break
        fi
        sleep 1
        [ $i -eq 30 ] && warn "等待 PX4 就绪超时，继续后续步骤（可能仍可正常工作）"
    done

    # 输出端口信息
    log "可通过以下端口连接 PX4:"
    log "  CompanionComputer / Offboard: udp://:14540"
    log "  GroundControlStation:         udp://:14550"
}

# --------------------------- 启动 QGC ---------------------------
start_qgc() {
    if [ ! -f "$QGC_APP" ]; then
        err "找不到 QGC: $QGC_APP"
        return 1
    fi
    if pgrep -f "QGroundControl" >/dev/null 2>&1; then
        warn "QGC 已在运行"
        return 0
    fi
    chmod +x "$QGC_APP" 2>/dev/null || true
    log "启动 QGroundControl: $QGC_APP"
    nohup "$QGC_APP" > "$WORKDIR/qgc.out" 2>&1 &
    echo $! > "$WORKDIR/qgc.pid"
    log "QGC 已启动 (PID=$(cat "$WORKDIR/qgc.pid"))"
    log "QGC 会自动连接 UDP 14550 并显示无人机位置/轨迹"
}

# --------------------------- 停止全部 ---------------------------
stop_all() {
    log "停止所有仿真组件..."
    for p in px4 qgc gzserver gzclient; do
        if [ -f "$WORKDIR/$p.pid" ]; then
            local pid=$(cat "$WORKDIR/$p.pid" 2>/dev/null)
            [ -n "$pid" ] && kill "$pid" 2>/dev/null && log "  $p (PID=$pid) 已停止"
            rm -f "$WORKDIR/$p.pid"
        fi
    done
    pkill -f "px4 .*init.d-posix" 2>/dev/null
    pkill -f "QGroundControl" 2>/dev/null
    pkill -f "gzserver" 2>/dev/null
    pkill -f "gzclient" 2>/dev/null
    pkill -f "sinfgui" 2>/dev/null
    log "已停止"
}

# --------------------------- 状态 ---------------------------
status() {
    echo "=== 仿真状态 ==="
    for p in px4 qgc gzserver gzclient; do
        if pgrep -f "$p" >/dev/null 2>&1; then
            echo -e "  ${G}●${N} $p"
        else
            echo -e "  ${R}○${N} $p"
        fi
    done
    echo "=== 端口监听 ==="
    for port in 14540 14550 14556 14557; do
        if ss -lun 2>/dev/null | grep -q ":$port "; then
            echo -e "  ${G}●${N} UDP $port"
        else
            echo -e "  ${R}○${N} UDP $port"
        fi
    done
}

# --------------------------- 主入口 ---------------------------
case "${1:-all}" in
    all)
        start_px4
        sleep 2
        start_qgc
        status
        echo
        log "全部启动完成！"
        log "现在可以运行 Qt 程序 (./dyq3)，开机按钮会自动连接 PX4。"
        ;;
    px4)  start_px4; status ;;
    qgc)  start_qgc ;;
    stop) stop_all ;;
    status) status ;;
    *) echo "用法: $0 [all|px4|qgc|stop|status]"; exit 1 ;;
esac
