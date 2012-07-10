/****************************************************************************
**
** Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/
**
** This file is part of the examples of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of Nokia Corporation and its Subsidiary(-ies) nor
**     the names of its contributors may be used to endorse or promote
**     products derived from this software without specific prior written
**     permission.
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

import QtQuick 2.0
import QtQuick.Particles 2.0
import "logic.js" as Logic

Item {
    id: gameOverScreen
    width: 320
    height: 400
    property GameCanvas gameCanvas

    Image {
        id: img
        source: "gfx/text-gameover.png"
        anchors.centerIn: parent
    }

    ParticleSystem {
        id: particles
    }

    ImageParticle {
        id: cloud
        anchors.fill: parent
        system: particles
        source: "gfx/cloud.png"
        alphaVariation: 0.25
        opacity: 0.25
        smooth: true
    }

    Wander {
        system: particles
        anchors.fill: parent
        xVariance: 100;
        pace: 1;
    }

    Emitter {
        id: cloudLeft
        system: particles
        width: 160
        height: 160
        anchors.right: parent.left
        emitRate: 0.5
        lifeSpan: 12000
        velocity: PointDirection{ x: 64; xVariation: 2; yVariation: 2 }
        size: 160
    }

    Emitter {
        id: cloudRight
        system: particles
        width: 160
        height: 160
        anchors.left: parent.right
        emitRate: 0.5
        lifeSpan: 12000
        velocity: PointDirection{ x: -64; xVariation: 2; yVariation: 2 }
        size: 160
    }

    Text {
        visible: gameCanvas != undefined
        text: "You saved " + gameCanvas.score + " fishes!"
        anchors.top: img.bottom
        anchors.topMargin: 12
        anchors.horizontalCenter: parent.horizontalCenter
        font.bold: true
        color: "#000000"
        opacity: 0.5
    }

    Image {
        source: "gfx/button-play.png"
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 0
        MouseArea {
            anchors.fill: parent
            onClicked: gameCanvas.gameOver = false//This will actually trigger the state change in main.qml
        }
    }
}