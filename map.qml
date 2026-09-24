import QtQuick 2.12

Rectangle {
    id: root
    anchors.fill: parent
    color: "blue"

    Text {
        text: "测试文本"
        color: "white"
        font.pixelSize: 32
        anchors.centerIn: parent
    }

    Rectangle {
        width: 100
        height: 100
        color: "red"
        anchors.centerIn: parent
    }
}
