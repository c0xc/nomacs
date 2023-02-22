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

#include "DkCropWidgets.h"

#include "DkBasicWidgets.h"
#include "DkSettings.h"
#include "DkUtils.h"
#include "DkMath.h"
#include "DkImageContainer.h"
#include "DkActionManager.h"

#pragma warning(push, 0)	// no warnings from includes
#include <QPainter>
#include <QPainterPath>
#include <QDebug>
#include <QMouseEvent>
#include <QComboBox>
#include <QPushButton>

#include <QMainWindow>
#include <QHBoxLayout>
#include <QSlider>
#pragma warning(pop)

namespace nmc {

// DkCropWidget --------------------------------------------------------------------
DkCropWidget::DkCropWidget(QWidget* parent /* = 0*/) : DkBaseViewPort(parent) {

	mViewportRect = canvas();
	mPanControl = QPointF(0, 0);
	mMinZoom = 1.0;
	mImgWithin = false;

	mCropArea.setWorldMatrix(&mWorldMatrix);
	mCropArea.setImageRect(&mImgViewRect);

	QAction* crop = new QAction(tr("Crop"), this);
	crop->setShortcut(Qt::Key_Return);
	addAction(crop);
	connect(crop, &QAction::triggered, this, &DkCropWidget::crop);

	DkActionManager& am = DkActionManager::instance();
	addActions(am.viewActions().toList());
}

void DkCropWidget::mouseDoubleClickEvent(QMouseEvent* ev) {

    crop();
    QWidget::mouseDoubleClickEvent(ev);
}

void DkCropWidget::mouseMoveEvent(QMouseEvent* ev) {


	if (ev->buttons() & Qt::LeftButton) {

		if (mCropArea.currentHandle() != DkCropArea::h_move)
			mCropArea.update(ev->pos());
		
		update();

		mLastMousePos = ev->pos();
	}

	// propagate moves
	if (mCropArea.currentHandle() == DkCropArea::h_move) {

		ev->ignore();
		DkBaseViewPort::mouseMoveEvent(ev);
	} 
	else {
		QCursor c = mCropArea.cursor(ev->pos());
		setCursor(c);
	}
}

void DkCropWidget::mousePressEvent(QMouseEvent* ev) {

	mLastMousePos = ev->pos();
	mCropArea.updateHandle(ev->pos());
	
	// propagate moves
	if (mCropArea.currentHandle() == DkCropArea::h_move)
		DkBaseViewPort::mousePressEvent(ev);
}

void DkCropWidget::mouseReleaseEvent(QMouseEvent* ev) {

	mCropArea.resetHandle();

	// propagate moves
	if (mCropArea.currentHandle() == DkCropArea::h_move)
		DkBaseViewPort::mouseReleaseEvent(ev);

	recenter();
}

void DkCropWidget::paintEvent(QPaintEvent* pe) {
	
	DkBaseViewPort::paintEvent(pe);

	// create path
	QPainterPath path;
	QRect canvas(
		geometry().x() - mStyle.pen().width(),
		geometry().y() - mStyle.pen().width(),
		geometry().width() + 2*mStyle.pen().width(),
		geometry().height() + 2*mStyle.pen().width()
		);
	path.addRect(canvas);

	QRectF crop = mCropArea.rect();
	path.addRect(crop);

	// init painter
	QPainter painter(viewport());

	painter.setPen(mStyle.pen());
	painter.setBrush(mStyle.bgBrush());
	painter.setRenderHint(QPainter::Antialiasing);

	painter.drawPath(path);

	// draw guides
	auto drawGuides = [&](int N = 3) {

		for (int idx = 0; idx < N; idx++) {

			// vertical lines
			double l = crop.left() + crop.width() / N * idx;
			QLineF line(QPointF(l, crop.top()), QPointF(l, crop.bottom()));
			painter.drawLine(line);

			// horizontal lines
			l = crop.top() + crop.height() / N * idx;
			line = QLineF(QPointF(crop.left(), l), QPointF(crop.right(), l));
			painter.drawLine(line);
		}
	};

	// highlight rectangle's corner
	auto drawCorners = [&](const QRectF& r, double width = 30) {

		painter.setPen(mStyle.cornerPen());

		QPointF p = r.topLeft();
		painter.drawLine(p, QPointF(p.x() + width, p.y()));
		painter.drawLine(p, QPointF(p.x(), p.y() + width));

		p = r.topRight();
		painter.drawLine(p, QPointF(p.x() - width, p.y()));
		painter.drawLine(p, QPointF(p.x(), p.y() + width));

		p = r.bottomRight();
		painter.drawLine(p, QPointF(p.x() - width, p.y()));
		painter.drawLine(p, QPointF(p.x(), p.y() - width));

		p = r.bottomLeft();
		painter.drawLine(p, QPointF(p.x() + width, p.y()));
		painter.drawLine(p, QPointF(p.x(), p.y() - width));
	};

	// draw decorations
	drawGuides(mIsRotating ? 10 : 3);
	drawCorners(crop);

	//// debug vis remove! -------------------------
	//painter.setBrush(Qt::NoBrush);

	//QPen debug;
	//debug.setColor(QColor(201, 60, 140));
	//debug.setWidth(3);
	//debug.setStyle(Qt::DashLine);
	//painter.setPen(debug);
	////painter.drawRect(mCropArea.imgViewRect());

	////QTransform t = mCropArea.transformCropToRect(winRect());
	////QRectF tr = t.mapRect(crop);
	////painter.drawRect(tr);
	//painter.drawRect(mViewportRect);
	//// debug vis remove! -------------------------
}

void DkCropWidget::resizeEvent(QResizeEvent* re) {

	if (re->oldSize() == re->size())
		return;

	updateViewRect(canvas());
	recenter();

	return QGraphicsView::resizeEvent(re);
}

void DkCropWidget::controlImagePosition(const QRect& r) {

	QRect cr = controlRect(r);
	
	QRect imgr = mWorldMatrix.mapRect(mImgViewRect).toRect();

	if (imgr.left() > cr.left())
		mWorldMatrix.translate(((double)cr.left() - imgr.left()) / mWorldMatrix.m11(), 0);

	if (imgr.top() > cr.top())
		mWorldMatrix.translate(0, ((double)cr.top() - imgr.top()) / mWorldMatrix.m11());

	if (imgr.right() < cr.right())
		mWorldMatrix.translate(((double)cr.right() - imgr.right()) / mWorldMatrix.m11(), 0);

	if (imgr.bottom() < cr.bottom())
		mWorldMatrix.translate(0, ((double)cr.bottom() - imgr.bottom()) / mWorldMatrix.m11());

	// update scene size (this is needed to make the scroll area work)
	if (DkSettingsManager::instance().param().display().showScrollBars)
		setSceneRect(getImageViewRect());
}

QRect DkCropWidget::canvas(int margin) const {

	return QRect(
		margin,
		margin,
		width() - 2*margin,
		height() - 2*margin
	);
}

void DkCropWidget::updateViewRect(const QRect& r) {

	if (r == mViewportRect)
		return;

	mViewportRect = r;
	updateImageMatrix();
	changeCursor();
}

void DkCropWidget::recenter() {

	mCropArea.recenter(canvas());
	updateViewRect(mCropArea.rect());
	controlImagePosition(); // TODO: do we need that?

	update();
}

void DkCropWidget::setImageContainer(const QSharedPointer<DkImageContainerT>& img) {
	mImage = img;

	if (img) {
		DkBaseViewPort::setImage(mImage->image());
		reset();
	}
}

void DkCropWidget::crop() {

	qDebug() << "cropping for you sir...";

	if (!mImage) {
		qWarning() << "cannot crop NULL image...";
		return;
	}

	QRect r = mCropArea.rect();
	r = mWorldMatrix.inverted().mapRect(r);
	r = mImgMatrix.inverted().mapRect(r);

	if (mCropArea.isActive() || mAngle != 0.0) {
		QTransform t;
		t.translate(-r.left(), -r.top());
		rotateTransform(t, mAngle, r.center());

		mImage->cropImage(r, t);
	}

	emit croppedSignal();
}

void DkCropWidget::reset() {

	mCropArea.reset();
	recenter();
	resetWorldMatrix();
}

void DkCropWidget::setWorldTransform(QTransform* worldMatrix) {
	mCropArea.setWorldMatrix(worldMatrix);
}

void DkCropWidget::setImageRect(const QRectF* rect) {
    mCropArea.setImageRect(rect);
}

void DkCropWidget::setVisible(bool visible) {

	if (!isVisible() && visible) {
		
		if (!mCropDock) {
			mCropDock = new QDockWidget(this);
			mCropDock->setObjectName("cropDock");
			mCropDock->setTitleBarWidget(new QWidget());

			DkCropToolBarNew* ctb = new DkCropToolBarNew(this);
			connect(ctb, &DkCropToolBarNew::rotateSignal, this, &DkCropWidget::rotate);
			connect(ctb, &DkCropToolBarNew::aspectRatioSignal, this, &DkCropWidget::setAspectRatio);
			connect(ctb, &DkCropToolBarNew::flipSignal, this, &DkCropWidget::flip);
			connect(ctb, &DkCropToolBarNew::isRotatingSignal, this, 
				[&](bool r) {
					mIsRotating = r; 
					update(); 
				});

			mCropDock->setWidget(ctb);
		}

		auto w = dynamic_cast<QMainWindow*>(DkUtils::getMainWindow());
		if (w) {
			w->addDockWidget(Qt::BottomDockWidgetArea, mCropDock);
		}
	}

	if (mCropDock)
		mCropDock->setVisible(visible);

	DkBaseViewPort::setVisible(visible);
}

void DkCropWidget::rotate(double angle) {
	
	mAngle = angle;
	update();
}

void DkCropWidget::setAspectRatio(const DkCropArea::Ratio& ratio) {

	mCropArea.setAspectRatio(ratio);
	recenter();
}

void DkCropWidget::flip() {
	
	mCropArea.flip();
	recenter();
}

bool DkCropArea::isActive() const {

	const QRect& r = rect();

	return qRound(r.width() / mWorldMatrix->m11()) != qRound(mImgViewRect->width()) ||
		qRound(r.height() / mWorldMatrix->m11()) != qRound(mImgViewRect->height());
}

void DkCropArea::setWorldMatrix(QTransform* matrix) {
    mWorldMatrix = matrix;
}

void DkCropArea::setImageRect(const QRectF* rect) {
    
	Q_ASSERT(rect);
	mImgViewRect = rect;
}

QRect DkCropArea::rect() const {
	
	// init the crop rect
	if (mCropRect.isNull()) {

		Q_ASSERT(mWorldMatrix);
		Q_ASSERT(mImgViewRect != nullptr);
		mCropRect = mWorldMatrix->mapRect(*mImgViewRect).toRect();
	}

	return mCropRect;
}

DkCropArea::Handle DkCropArea::getHandle(const QPoint& pos, int proximity) const {
	
	if (mCurrentHandle != h_no_handle)
		return mCurrentHandle;

	int pxs = proximity * proximity;
	QRect r = rect();

	// squared euclidean distance
	auto dist = [](const QPoint& p1, const QPoint& p2) {

		int dx = p1.x() - p2.x();
		int dy = p1.y() - p2.y();

		return dx * dx + dy * dy;
	};

	if (dist(r.topLeft(), pos) < pxs)
		return Handle::h_top_left;
	else if (dist(r.bottomRight(), pos) < pxs)
		return Handle::h_bottom_right;
	else if (dist(r.topRight(), pos) < pxs)
		return Handle::h_top_right;
	else if (dist(r.bottomLeft(), pos) < pxs)
		return Handle::h_bottom_left;
	else if (qAbs(r.left() - pos.x()) < proximity)
		return Handle::h_left;
	else if (qAbs(r.right() - pos.x()) < proximity)
		return Handle::h_right;
	else if (qAbs(r.top() - pos.y()) < proximity)
		return Handle::h_top;
	else if (qAbs(r.bottom() - pos.y()) < proximity)
		return Handle::h_bottom;
	else if (r.contains(pos))
		return Handle::h_move;

	return Handle::h_no_handle;
}

//QPointF DkCropArea::mapToImage(const QPoint& pos) const {
//	Q_ASSERT(mWorldMatrix);
//	return mWorldMatrix->inverted().map(pos);
//}

void DkCropArea::updateHandle(const QPoint& pos) {

	mCurrentHandle = getHandle(pos);
}

void DkCropArea::resetHandle() {
	mCurrentHandle = h_no_handle;
}

QCursor DkCropArea::cursor(const QPoint& pos) const {
	
	Handle h = getHandle(pos);

	if (h == h_top_left ||
		h == h_bottom_right) {
		return Qt::SizeFDiagCursor;
	}
	else if (h == h_top_right ||
		h == h_bottom_left) {
		return Qt::SizeBDiagCursor;
	}
	else if (h == h_left ||
		h == h_right) {
		return Qt::SizeHorCursor;
	}
	else if (h == h_top ||
		h == h_bottom) {
		return Qt::SizeVerCursor;
	}
	else if (h == h_move) {
		return Qt::OpenHandCursor;
	}

	return QCursor();
}

DkCropArea::Handle DkCropArea::currentHandle() const {
	return mCurrentHandle;
}

void DkCropArea::setAspectRatio(const DkCropArea::Ratio& r) {
	mRatio = r;
	applyRatio(r);
}

void DkCropArea::applyRatio(const DkCropArea::Ratio& r) {
	
	// do nothing
	if (r == r_free)
		return;

	auto enforceRatio = [&](double ratio) {

		const QRect& r = rect();
		int cl = isLandscape() ? r.width() : r.height();
		int ns = qRound(cl/ratio);

		QPoint oc = r.center();

		if (isLandscape())
			mCropRect.setHeight(ns);
		else
			mCropRect.setWidth(ns);

		// center it
		mCropRect.moveCenter(oc);
	};

	enforceRatio(toRatio(r));
}

void DkCropArea::flip() {

	const QRect& r = rect();

	QPoint oc = r.center();
	int ow = r.width();
	mCropRect.setWidth(r.height());
	mCropRect.setHeight(ow);
	mCropRect.moveCenter(oc);
}

double DkCropArea::toRatio(const DkCropArea::Ratio& r) {

	switch (r) {
	case Ratio::r_square:
		return 1.0;
	case Ratio::r_original:
		return mOriginalRatio;
	case Ratio::r_16_9:
		return 16 / 9.0;
	case Ratio::r_4_3:
		return 4 / 3.0;
	case Ratio::r_3_2:
		return 3 / 2.0;
	}

	qWarning() << "illegal ratio: " << r;

	return 1.0;
}

void DkCropArea::move(const QPoint& dxy) {

	mCropRect.moveCenter(mCropRect.center()-dxy);
}

//void DkCropArea::rotate(double angle) {
//
//	Q_ASSERT(mImgViewRect);
//	Q_ASSERT(mWorldMatrix);
//	
//	QPointF c = mCropRect.center();
//
//	//// rotate image around center...
//	//mWorldMatrix->translate(c.x(), c.y());
//	//mWorldMatrix->rotate(angle + getAngle());
//	//mWorldMatrix->translate(-c.x(), -c.y());
//}

void DkCropArea::reset() {

	mCurrentHandle = Handle::h_no_handle;
	mCropRect = QRect();
	mOriginalRatio = (double)mImgViewRect->width() / mImgViewRect->height();
}

void DkCropArea::recenter(const QRectF& target) {

	if (target.isNull())
		return;

	QTransform t = transformCropToRect(target);

	mCropRect = t.mapRect(rect());
	*mWorldMatrix = *mWorldMatrix * t;
}

QTransform DkCropArea::transformCropToRect(const QRectF& target) const {

	QRectF crop = rect();

	if (crop.isNull())
		return QTransform();

	double scale = qMin(target.width() / crop.width(), target.height() / crop.height());

	if (scale == 0.0)
		return QTransform();

	QTransform t;
	t.scale(scale, scale);

	QRectF cs = t.mapRect(crop);
	QPointF dxy(target.center() - cs.center());
	dxy /= scale;

	t.translate(dxy.x(), dxy.y());

	return t;
}

bool DkCropArea::isLandscape() const {

	const QRect r = rect();
	return r.width() >= r.height();
}

void DkCropArea::update(const QPoint& pos) {

	if (mCurrentHandle == h_no_handle)
		return;

	// enforce aspect ratios
	auto enforce = [&](const QPoint& p, const QPoint& origin, bool principalDiagonal = true) -> QPoint {

		if (mRatio == Ratio::r_free)
			return p;

		float ar = (float)toRatio(mRatio);

		if (!principalDiagonal)
			ar *= -1.0f;

		// normalized diagonal
		DkVector d = isLandscape() ? DkVector(ar, 1) : DkVector(1, ar);
		d /= d.norm();

		// project p onto the diagonal
		DkVector pv = DkVector(p) - DkVector(origin);
		DkVector lp = d * pv.scalarProduct(d);

		return lp.toQPointF().toPoint() + origin;
	};

	auto clip = [&](QPoint& p) {

		// keep position within the image
		QRect ir = mWorldMatrix->mapRect(*mImgViewRect).toRect();
		if (p.x() > ir.right())
			p.setX(ir.right());
		if (p.x() < ir.left())
			p.setX(ir.left());
		if (p.y() > ir.bottom())
			p.setY(ir.bottom());
		if (p.y() < ir.top())
			p.setY(ir.top());
	};

	QPoint p = pos;

	// fix the other coordinate
	switch (mCurrentHandle) {
	case Handle::h_left:
		p.setY(mCropRect.bottom()); break;
	case Handle::h_right:
		p.setY(mCropRect.bottom()); break;
	case Handle::h_top:
		p.setX(mCropRect.right()); break;
	case Handle::h_bottom:
		p.setX(mCropRect.right()); break;
	default: break;
		// do nothing...
	}

	// update corners
	switch (mCurrentHandle) {

	case Handle::h_top_left: {
		p = enforce(p, mCropRect.bottomRight());
		clip(p);
		mCropRect.setTopLeft(p);
		break;
	}
	case Handle::h_top:
	case Handle::h_top_right: {
		p = enforce(p, mCropRect.bottomLeft(), false);
		clip(p);
		mCropRect.setTopRight(p);
		break;
	}
	case Handle::h_bottom:
	case Handle::h_right:
	case Handle::h_bottom_right: {
		p = enforce(p, mCropRect.topLeft());
		clip(p);
		mCropRect.setBottomRight(p);
		break;
	}
	case Handle::h_left:
	case Handle::h_bottom_left: {
		p = enforce(p, mCropRect.topRight(), false);
		clip(p);
		mCropRect.setBottomLeft(p);
		break;
	}
	}

	if (!mCropRect.isValid()) {
		mCropRect.setWidth(qMax(mCropRect.width(), 1));
		mCropRect.setHeight(qMax(mCropRect.height(), 1));
	}
}

// -------------------------------------------------------------------- DkCropStyle 
DkCropStyle::DkCropStyle(const QColor& dark, const QColor& light) {

	mDarkColor = dark;
	mLightColor = light;
}

QBrush DkCropStyle::bgBrush() const {

	QColor bb = mDarkColor;
	bb.setAlpha(qRound(mOpacity * 255));

	return bb;
}

QColor DkCropStyle::lightColor() const {
	return mLightColor;
}

QPen DkCropStyle::pen() const {

	QPen p(mLightColor, mLineWidth);
	p.setCosmetic(true);

	return p;
}

QPen DkCropStyle::cornerPen() const {

	QPen p(mLightColor, mLineWidth*2);
	p.setCosmetic(true);

	return p;
}

DkCropToolBarNew::DkCropToolBarNew(QWidget* parent) : QWidget(parent) {

	createLayout();
	QMetaObject::connectSlotsByName(this);
}

void DkCropToolBarNew::createLayout() {

	mRatioBox = new QComboBox(this);
	mRatioBox->setObjectName("ratioBox");

	// dear future me: we can use this with C++20:
	// using enum DkCropArea;
	mRatioBox->addItem(DkImage::loadIcon(":/nomacs/img/aspect-ratio.svg"), tr("Aspect Ratio"), DkCropArea::Ratio::r_free);
	mRatioBox->addItem(tr("Free"), DkCropArea::Ratio::r_free);
	mRatioBox->addItem(tr("Original"), DkCropArea::Ratio::r_original);
	mRatioBox->addItem(tr("Square"), DkCropArea::Ratio::r_square);
	mRatioBox->addItem(tr("16:9"), DkCropArea::Ratio::r_16_9);
	mRatioBox->addItem(tr("4:3"), DkCropArea::Ratio::r_4_3);
	mRatioBox->addItem(tr("3:2"), DkCropArea::Ratio::r_3_2);

	QPushButton* flipButton = new QPushButton(tr("Flip"), this);

	DkDoubleSlider* angleSlider = new DkDoubleSlider("", this);
	angleSlider->setTickInterval(1/90.0);
	angleSlider->setMinimum(-45);
	angleSlider->setMaximum(45);
	angleSlider->setValue(0.0);
	angleSlider->setMaximumWidth(400);

	auto s = angleSlider->getSlider();
	connect(s, &QSlider::sliderPressed, this, [&]() { emit isRotatingSignal(true); });
	connect(s, &QSlider::sliderReleased, this, [&]() { emit isRotatingSignal(false); });
	connect(angleSlider, &DkDoubleSlider::valueChanged, this, &DkCropToolBarNew::rotateSignal);
	connect(flipButton, &QPushButton::clicked, this, &DkCropToolBarNew::flipSignal);

	QHBoxLayout* l = new QHBoxLayout(this);
	l->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);

	l->addStretch();
	l->addWidget(mRatioBox);
	l->addWidget(flipButton);
	l->addWidget(angleSlider);
	l->addStretch();
}

void DkCropToolBarNew::on_ratioBox_currentIndexChanged(int idx) const {

	auto r = static_cast<DkCropArea::Ratio>(mRatioBox->itemData(idx).toInt());
	emit aspectRatioSignal(r);
}

}