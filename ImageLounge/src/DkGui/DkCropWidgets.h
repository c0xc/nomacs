/*******************************************************************************************************
 nomacs is a fast and small image viewer with the capability of synchronizing multiple instances
 
 Copyright (C) 2011-2016 Markus Diem <markus@nomacs.org>
 Copyright (C) 2011-2016 Stefan Fiel <stefan@nomacs.org>
 Copyright (C) 2011-2016 Florian Kleber <florian@nomacs.org>

 This file is part of nomacs.

 nomacs is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 nomacs is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.

 related links:
 [1] http://www.nomacs.org/
 [2] https://github.com/nomacs/
 [3] http://download.nomacs.org
 *******************************************************************************************************/

#pragma once

#include "DkBaseWidgets.h"

#pragma warning(push, 0)	// no warnings from includes
#include <QPen>
#include <QBrush>
#pragma warning(pop)

#ifndef DllExport
#ifdef DK_DLL_EXPORT
#define DllExport Q_DECL_EXPORT
#elif DK_DLL_IMPORT
#define DllExport Q_DECL_IMPORT
#else
#define DllExport Q_DECL_IMPORT
#endif
#endif

// Qt defines

namespace nmc {

// nomacs defines
    
class DkCropArea {

public:
    enum Handle : int {

        h_no_handle = 0,
        h_top_left,
        h_top_right,
        h_bottom_right,
        h_bottom_left,
        h_top,
        h_bottom,
        h_left,
        h_right,
        h_move,

        h_end
    };

    void setWorldMatrix(const QTransform* matrix);
    void setImageMatrix(const QTransform* matrix);
    void setImageRect(const QRectF* rect);

    QRectF cropViewRect() const;

    void updateHandle(const QPoint& pos);
    void resetHandle();
    QCursor cursor(const QPoint& pos) const;
    Handle currentHandle() const;

    void update(const QPoint& pos);
    void move(const QPoint& dxy);

    void reset();

private:
    const QTransform* mWorldMatrix = nullptr;
    const QTransform* mImgMatrix = nullptr;
    const QRectF* mImgViewRect = nullptr;

    // the crop rect is kept in image coordinates
    mutable QRectF mCropRect;
    DkCropArea::Handle mCurrentHandle = DkCropArea::Handle::h_no_handle;

    Handle getHandle(const QPoint& pos, int proximity = 15) const;
    QPointF mapToImage(const QPoint& pos) const;
};

class DkCropStyle {

public:

    DkCropStyle(
        const QColor& dark = QColor(0, 0, 0), 
        const QColor& light = QColor(255, 255, 255));

    QBrush bgBrush() const;
    QPen pen() const;
    QPen cornerPen() const;

    QColor lightColor() const;
    
private:
    QColor mDarkColor;
    QColor mLightColor;
    int mLineWidth = 2;
    double mOpacity = 0.8;

};

class DkCropWidget : public DkFadeWidget {
    Q_OBJECT

public:
    DkCropWidget(QWidget* parent = 0, Qt::WindowFlags f = Qt::WindowFlags());

    void reset();

    void setTransforms(const QTransform* worldMatrix, const QTransform* imgMatrix);
    void setImageRect(const QRectF* rect);

public slots:
    void crop(bool cropToMetadata = false);
    virtual void setVisible(bool visible) override;

signals:
    void cropImageSignal(const QRectF& rect, bool cropToMetaData = false) const;
    void hideSignal() const;

protected:
    void mouseDoubleClickEvent(QMouseEvent* ev) override;
    void mouseMoveEvent(QMouseEvent* ev) override;
    void mousePressEvent(QMouseEvent* ev) override;
    void mouseReleaseEvent(QMouseEvent* ev) override;

    void paintEvent(QPaintEvent* pe) override;

    DkCropStyle mStyle;

    DkCropArea mCropArea;
    QRectF mRect; // TODO: remove?
    QPoint mLastMousePos;
};

}