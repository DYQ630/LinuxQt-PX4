#include "widget.h"

#include <QApplication>
#include <QByteArray>

int main(int argc, char *argv[])
{
    // 关键：禁用 Chromium GPU 合成，强制软件渲染
    // 解决 Linux/X11 下 QWebEngineView 作为嵌套子控件时 GPU 合成器无法正确渲染的问题
    // 顶层窗口（如 mapDialog）不受影响，但嵌入到子控件层级的 QWebEngineView 必须用软件渲染
    qputenv("QTWEBENGINE_CHROMIUM_FLAGS", QByteArray("--disable-gpu --disable-gpu-compositing"));

    QApplication a(argc, argv);
    
    QString styleSheet = R"(
        QWidget {
            background: qlineargradient(x1: 0, y1: 0, x2: 1, y2: 1, stop: 0 #1a4a5e, stop: 0.5 #2a6a7e, stop: 1 #1a4a5e);
            color: #88ffff;
            font-family: "Microsoft YaHei", "Noto Sans CJK SC", "WenQuanYi Micro Hei", "PingFang SC", "Heiti SC", sans-serif;
        }
        
        QFrame#topFrame {
            border-bottom: 2px solid #88ffff;
            padding-bottom: 10px;
        }
        
        QLabel#titleLabel {
            font-size: 18px;
            font-weight: bold;
            color: #aaffff;
            text-shadow: 0 0 10px #88ffff;
        }
        
        QLabel#serialLabel {
            font-size: 12px;
            color: #66eeee;
            opacity: 0.7;
        }
        
        QFrame#firstRowFrame, QFrame#secondRowFrame, QFrame#thirdRowFrame, 
        QFrame#fourthRowFrame, QFrame#fifthRowFrame {
            background: rgba(26, 74, 94, 0.8);
            border: 1px solid #88ffff;
            border-radius: 8px;
        }
        
        QFrame#infoFrame {
            background: rgba(26, 74, 94, 0.6);
            border: 1px solid #88ffff;
            border-radius: 6px;
        }
        
        QPushButton#roundButton {
            width: 55px;
            height: 55px;
            border-radius: 27px;
            border: 2px solid #88ffff;
            background: rgba(136, 255, 255, 0.15);
            color: #88ffff;
            font-size: 12px;
            font-weight: bold;
            text-align: center;
        }
        
        QPushButton#roundButton:hover {
            background: rgba(136, 255, 255, 0.25);
            box-shadow: 0 0 15px #88ffff;
        }
        
        QPushButton#roundButton:pressed {
            background: rgba(136, 255, 255, 0.35);
        }
        
        QPushButton#largeRoundButton {
            width: 65px;
            height: 65px;
            border-radius: 32px;
            border: 2px solid #88ffff;
            background: rgba(136, 255, 255, 0.15);
            color: #88ffff;
            font-size: 13px;
            font-weight: bold;
            text-align: center;
        }
        
        QPushButton#largeRoundButton:hover {
            background: rgba(136, 255, 255, 0.25);
            box-shadow: 0 0 20px #88ffff;
        }
        
        QPushButton#rectButton {
            padding: 12px 30px;
            border-radius: 6px;
            border: 1px solid #88ffff;
            background: rgba(136, 255, 255, 0.15);
            color: #88ffff;
            font-size: 14px;
            font-weight: bold;
        }
        
        QPushButton#rectButton:hover {
            background: rgba(136, 255, 255, 0.25);
        }
        
        QPushButton#ellipseButton {
            width: 80px;
            height: 40px;
            border-radius: 20px;
            border: 1px solid #88ffff;
            background: rgba(136, 255, 255, 0.15);
            color: #88ffff;
            font-size: 13px;
            font-weight: bold;
            text-align: center;
        }
        
        QPushButton#ellipseButton:hover {
            background: rgba(136, 255, 255, 0.25);
            box-shadow: 0 0 10px #88ffff;
        }
        
        QPushButton#smallBtn {
            padding: 5px 15px;
            border-radius: 4px;
            border: 1px solid #88ffff;
            background: rgba(136, 255, 255, 0.15);
            color: #88ffff;
            font-size: 12px;
        }
        
        QPushButton#smallBtn:hover {
            background: rgba(136, 255, 255, 0.25);
        }
        
        QLineEdit#destinationInput {
            width: 200px;
            height: 40px;
            padding: 0 15px;
            border: 1px solid #88ffff;
            border-radius: 6px;
            background: rgba(26, 74, 94, 0.8);
            color: #88ffff;
            font-size: 14px;
        }
        
        QLineEdit#destinationInput:focus {
            border-color: #aaffff;
            box-shadow: 0 0 10px rgba(170, 255, 255, 0.5);
        }
        
        QLabel#dataLabel {
            font-size: 14px;
            color: #88ffff;
        }
        
        QLabel#timeLabel {
            font-size: 12px;
            color: #66eeee;
            opacity: 0.8;
        }
        
        QLabel#batteryPercent {
            font-size: 24px;
            font-weight: bold;
            color: #88ffcc;
            text-align: center;
        }
        
        QFrame#batteryBar1, QFrame#batteryBar2, QFrame#batteryBar3 {
            background: #88ffcc;
            border-radius: 2px;
        }
        
        QFrame#batteryBar4 {
            background: #ffff88;
            border-radius: 2px;
        }
        
        QFrame#batteryBar5 {
            background: #ff8888;
            border-radius: 2px;
        }
    )";
    
    a.setStyleSheet(styleSheet);
    
    Widget w;
    //w.setFixedSize(840, 540);     // 设置窗口固定大小为宽840像素，高540像素
    w.show();
    return a.exec();
}
