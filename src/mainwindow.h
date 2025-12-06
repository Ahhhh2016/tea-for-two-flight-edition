#pragma once

#include <QMainWindow>
#include <QCheckBox>
#include <QSlider>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include "realtime.h"
#include "utils/aspectratiowidget/aspectratiowidget.hpp"

class MainWindow : public QWidget
{
    Q_OBJECT

public:
    void initialize();
    void finish();

private:
    void connectUIElements();
    void connectParam1();
    void connectParam2();
    void connectNear();
    void connectFar();
    void connectFocusDist();
    void connectFocusRange();
    void connectMaxBlurRadius();

    // From old Project 6
    // void connectPerPixelFilter();
    // void connectKernelBasedFilter();

    void connectUploadFile();
    void connectSaveImage();
    void connectExtraCredit();

    Realtime *realtime;
    AspectRatioWidget *aspectRatioWidget;

    // From old Project 6
    // QCheckBox *filter1;
    // QCheckBox *filter2;

    QPushButton *uploadFile;
    QPushButton *saveImage;
    QSlider *p1Slider;
    QSlider *p2Slider;
    QSpinBox *p1Box;
    QSpinBox *p2Box;
    QSlider *nearSlider;
    QSlider *farSlider;
    QDoubleSpinBox *nearBox;
    QDoubleSpinBox *farBox;
    // Depth of Field controls
    QSlider *focusDistSlider;
    QSlider *focusRangeSlider;
    QSlider *maxBlurRadiusSlider;
    QDoubleSpinBox *focusDistBox;
    QDoubleSpinBox *focusRangeBox;
    QDoubleSpinBox *maxBlurRadiusBox;

    // Extra Credit:
    QCheckBox *ec1;
    QCheckBox *ec2;
    QCheckBox *ec3;
    QCheckBox *ec4;
    // Rendering toggles
    QCheckBox *fog;

private slots:
    // From old Project 6
    // void onPerPixelFilter();
    // void onKernelBasedFilter();

    void onUploadFile();
    void onSaveImage();
    void onValChangeP1(int newValue);
    void onValChangeP2(int newValue);
    void onValChangeNearSlider(int newValue);
    void onValChangeFarSlider(int newValue);
    void onValChangeNearBox(double newValue);
    void onValChangeFarBox(double newValue);
    void onValChangeFocusDistSlider(int newValue);
    void onValChangeFocusDistBox(double newValue);
    void onValChangeFocusRangeSlider(int newValue);
    void onValChangeFocusRangeBox(double newValue);
    void onValChangeMaxBlurRadiusSlider(int newValue);
    void onValChangeMaxBlurRadiusBox(double newValue);

    // Extra Credit:
    void onExtraCredit1();
    void onExtraCredit2();
    void onExtraCredit3();
    void onExtraCredit4();
    // Rendering toggles:
    void onFogToggled(bool checked);
};
