#include "mainwindow.h"
#include "settings.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFileDialog>
#include <QSettings>
#include <QLabel>
#include <QGroupBox>
#include <iostream>

void MainWindow::initialize() {
    realtime = new Realtime;
    aspectRatioWidget = new AspectRatioWidget(this);
    aspectRatioWidget->setAspectWidget(realtime, 3.f/4.f);
    QHBoxLayout *hLayout = new QHBoxLayout; // horizontal alignment
    QHBoxLayout *controlsLayout = new QHBoxLayout(); // two control columns
    QVBoxLayout *vLayout = new QVBoxLayout(); // first column
    QVBoxLayout *vLayout2 = new QVBoxLayout(); // second column
    vLayout->setAlignment(Qt::AlignTop);
    vLayout2->setAlignment(Qt::AlignTop);
    controlsLayout->addLayout(vLayout);
    controlsLayout->addLayout(vLayout2);
    hLayout->addLayout(controlsLayout);
    hLayout->addWidget(aspectRatioWidget, 1);
    this->setLayout(hLayout);

    // Create labels in sidebox
    QFont font;
    font.setPointSize(12);
    font.setBold(true);
    QLabel *tesselation_label = new QLabel(); // Parameters label
    tesselation_label->setText("Tesselation");
    tesselation_label->setFont(font);
    QLabel *camera_label = new QLabel(); // Camera label
    camera_label->setText("Camera");
    camera_label->setFont(font);

    // From old Project 6
    // QLabel *filters_label = new QLabel(); // Filters label
    // filters_label->setText("Filters");
    // filters_label->setFont(font);

    QLabel *ec_label = new QLabel(); // Extra Credit label
    ec_label->setText("Extra Features");
    ec_label->setFont(font);
    QLabel *param1_label = new QLabel(); // Parameter 1 label
    param1_label->setText("Parameter 1:");
    QLabel *param2_label = new QLabel(); // Parameter 2 label
    param2_label->setText("Parameter 2:");
    QLabel *near_label = new QLabel(); // Near plane label
    near_label->setText("Near Plane:");
    QLabel *far_label = new QLabel(); // Far plane label
    far_label->setText("Far Plane:");

    // Depth of Field labels
    QLabel *dof_label = new QLabel();
    dof_label->setText("Depth of Field");
    dof_label->setFont(font);
    QLabel *focusDist_label = new QLabel();
    focusDist_label->setText("Focus Distance:");
    QLabel *focusRange_label = new QLabel();
    focusRange_label->setText("Focus Range:");
    QLabel *maxBlur_label = new QLabel();
    maxBlur_label->setText("Max Blur Radius (px):");


    // From old Project 6
    // // Create checkbox for per-pixel filter
    // filter1 = new QCheckBox();
    // filter1->setText(QStringLiteral("Per-Pixel Filter"));
    // filter1->setChecked(false);
    // // Create checkbox for kernel-based filter
    // filter2 = new QCheckBox();
    // filter2->setText(QStringLiteral("Kernel-Based Filter"));
    // filter2->setChecked(false);

    // Create file uploader for scene file
    uploadFile = new QPushButton();
    uploadFile->setText(QStringLiteral("Upload Scene File"));
    
    saveImage = new QPushButton();
    saveImage->setText(QStringLiteral("Save Image"));

    // Creates the boxes containing the parameter sliders and number boxes
    QGroupBox *p1Layout = new QGroupBox(); // horizonal slider 1 alignment
    QHBoxLayout *l1 = new QHBoxLayout();
    QGroupBox *p2Layout = new QGroupBox(); // horizonal slider 2 alignment
    QHBoxLayout *l2 = new QHBoxLayout();

    // Create slider controls to control parameters
    p1Slider = new QSlider(Qt::Orientation::Horizontal); // Parameter 1 slider
    p1Slider->setTickInterval(1);
    p1Slider->setMinimum(1);
    p1Slider->setMaximum(25);
    p1Slider->setValue(1);

    p1Box = new QSpinBox();
    p1Box->setMinimum(1);
    p1Box->setMaximum(25);
    p1Box->setSingleStep(1);
    p1Box->setValue(1);

    p2Slider = new QSlider(Qt::Orientation::Horizontal); // Parameter 2 slider
    p2Slider->setTickInterval(1);
    p2Slider->setMinimum(1);
    p2Slider->setMaximum(25);
    p2Slider->setValue(1);

    p2Box = new QSpinBox();
    p2Box->setMinimum(1);
    p2Box->setMaximum(25);
    p2Box->setSingleStep(1);
    p2Box->setValue(1);

    // Adds the slider and number box to the parameter layouts
    l1->addWidget(p1Slider);
    l1->addWidget(p1Box);
    p1Layout->setLayout(l1);

    l2->addWidget(p2Slider);
    l2->addWidget(p2Box);
    p2Layout->setLayout(l2);

    // Creates the boxes containing the camera sliders and number boxes
    QGroupBox *nearLayout = new QGroupBox(); // horizonal near slider alignment
    QHBoxLayout *lnear = new QHBoxLayout();
    QGroupBox *farLayout = new QGroupBox(); // horizonal far slider alignment
    QHBoxLayout *lfar = new QHBoxLayout();

    // Create slider controls to control near/far planes
    nearSlider = new QSlider(Qt::Orientation::Horizontal); // Near plane slider
    nearSlider->setTickInterval(1);
    nearSlider->setMinimum(1);
    nearSlider->setMaximum(1000);
    nearSlider->setValue(10);

    nearBox = new QDoubleSpinBox();
    nearBox->setMinimum(0.01f);
    nearBox->setMaximum(10.f);
    nearBox->setSingleStep(0.1f);
    nearBox->setValue(0.1f);

    farSlider = new QSlider(Qt::Orientation::Horizontal); // Far plane slider
    farSlider->setTickInterval(1);
    farSlider->setMinimum(1000);
    farSlider->setMaximum(10000);
    farSlider->setValue(10000);

    farBox = new QDoubleSpinBox();
    farBox->setMinimum(10.f);
    farBox->setMaximum(100.f);
    farBox->setSingleStep(0.1f);
    farBox->setValue(100.f);

    // Adds the slider and number box to the parameter layouts
    lnear->addWidget(nearSlider);
    lnear->addWidget(nearBox);
    nearLayout->setLayout(lnear);

    lfar->addWidget(farSlider);
    lfar->addWidget(farBox);
    farLayout->setLayout(lfar);

    // Creates the boxes containing DOF sliders and number boxes
    QGroupBox *focusDistLayout = new QGroupBox();
    QHBoxLayout *lfd = new QHBoxLayout();
    QGroupBox *focusRangeLayout = new QGroupBox();
    QHBoxLayout *lfr = new QHBoxLayout();
    QGroupBox *maxBlurLayout = new QGroupBox();
    QHBoxLayout *lmb = new QHBoxLayout();

    // Focus Distance controls (0.10 - 20.00)
    focusDistSlider = new QSlider(Qt::Orientation::Horizontal);
    focusDistSlider->setTickInterval(1);
    focusDistSlider->setMinimum(10);   // 0.10
    focusDistSlider->setMaximum(2000); // 20.00
    focusDistSlider->setValue(int(settings.focusDist * 100.f));
    focusDistBox = new QDoubleSpinBox();
    focusDistBox->setMinimum(0.10f);
    focusDistBox->setMaximum(20.0f);
    focusDistBox->setSingleStep(0.1f);
    focusDistBox->setValue(settings.focusDist);
    lfd->addWidget(focusDistSlider);
    lfd->addWidget(focusDistBox);
    focusDistLayout->setLayout(lfd);

    // Focus Range controls (0.10 - 5.00)
    focusRangeSlider = new QSlider(Qt::Orientation::Horizontal);
    focusRangeSlider->setTickInterval(1);
    focusRangeSlider->setMinimum(10);  // 0.10
    focusRangeSlider->setMaximum(500); // 5.00
    focusRangeSlider->setValue(int(settings.focusRange * 100.f));
    focusRangeBox = new QDoubleSpinBox();
    focusRangeBox->setMinimum(0.10f);
    focusRangeBox->setMaximum(5.0f);
    focusRangeBox->setSingleStep(0.05f);
    focusRangeBox->setValue(settings.focusRange);
    lfr->addWidget(focusRangeSlider);
    lfr->addWidget(focusRangeBox);
    focusRangeLayout->setLayout(lfr);

    // Max Blur Radius controls (0.0 - 40.0 px)
    maxBlurRadiusSlider = new QSlider(Qt::Orientation::Horizontal);
    maxBlurRadiusSlider->setTickInterval(1);
    maxBlurRadiusSlider->setMinimum(0);    // 0.0
    maxBlurRadiusSlider->setMaximum(4000); // 40.0
    maxBlurRadiusSlider->setValue(int(settings.maxBlurRadius * 100.f));
    maxBlurRadiusBox = new QDoubleSpinBox();
    maxBlurRadiusBox->setMinimum(0.0f);
    maxBlurRadiusBox->setMaximum(40.0f);
    maxBlurRadiusBox->setSingleStep(0.5f);
    maxBlurRadiusBox->setValue(settings.maxBlurRadius);
    lmb->addWidget(maxBlurRadiusSlider);
    lmb->addWidget(maxBlurRadiusBox);
    maxBlurLayout->setLayout(lmb);



    // Extra Credit:
    ec1 = new QCheckBox();
    ec1->setText(QStringLiteral("ALOD1: number of objects"));
    ec1->setChecked(false);

    ec2 = new QCheckBox();
    ec2->setText(QStringLiteral("ALOD2: distance to the camera"));
    ec2->setChecked(false);

    ec3 = new QCheckBox();
    ec3->setText(QStringLiteral("Open"));
    ec3->setChecked(false);

    ec4 = new QCheckBox();
    ec4->setText(QStringLiteral("Motion blur"));
    ec4->setChecked(false);

    // Fog
    fog = new QCheckBox();
    fog->setText(QStringLiteral("Fog"));
    fog->setChecked(settings.fogEnabled);

	// Fullscreen Scene toggle
	toggleScene = new QPushButton();
	{
		// Initialize text based on current setting
		QString label = (settings.fullscreenScene == FullscreenScene::Water)
			? QStringLiteral("Scene: Water")
			: QStringLiteral("Scene: Rainforest");
		toggleScene->setText(label);
	}

    vLayout->addWidget(uploadFile);
    vLayout->addWidget(saveImage);
    vLayout->addWidget(tesselation_label);
    vLayout->addWidget(param1_label);
    vLayout->addWidget(p1Layout);
    vLayout->addWidget(param2_label);
    vLayout->addWidget(p2Layout);
    vLayout->addWidget(camera_label);
    vLayout->addWidget(near_label);
    vLayout->addWidget(nearLayout);
    vLayout->addWidget(far_label);
    vLayout->addWidget(farLayout);


    // Extra Credit:
    vLayout2->addWidget(ec_label);
    vLayout2->addWidget(ec1);
    vLayout2->addWidget(ec2);
    vLayout2->addWidget(ec4);
    vLayout2->addWidget(fog);
	vLayout2->addWidget(toggleScene);

    // Rainforest Intensity
    QLabel *iqIntensity_label = new QLabel();
    iqIntensity_label->setText("Rainforest Intensity:");
    QGroupBox *iqIntensityLayout = new QGroupBox();
    QHBoxLayout *liq = new QHBoxLayout();
    iqIntensitySlider = new QSlider(Qt::Orientation::Horizontal);
    iqIntensitySlider->setTickInterval(1);
    iqIntensitySlider->setMinimum(0);     // 0.00
    iqIntensitySlider->setMaximum(100);   // 1.00
    iqIntensitySlider->setValue(int(settings.rainforestIntensity * 100.f));
    iqIntensityBox = new QDoubleSpinBox();
    iqIntensityBox->setMinimum(0.0);
    iqIntensityBox->setMaximum(1.0);
    iqIntensityBox->setSingleStep(0.01);
    iqIntensityBox->setValue(settings.rainforestIntensity);
    liq->addWidget(iqIntensitySlider);
    liq->addWidget(iqIntensityBox);
    iqIntensityLayout->setLayout(liq);
    vLayout2->addWidget(iqIntensity_label);
    vLayout2->addWidget(iqIntensityLayout);

    // DOF block to second column
    vLayout2->addWidget(dof_label);
    vLayout2->addWidget(ec3);
    vLayout2->addWidget(focusDist_label);
    vLayout2->addWidget(focusDistLayout);
    vLayout2->addWidget(focusRange_label);
    vLayout2->addWidget(focusRangeLayout);
    vLayout2->addWidget(maxBlur_label);
    vLayout2->addWidget(maxBlurLayout);
    

    // From old Project 6
    // vLayout->addWidget(filters_label);
    // vLayout->addWidget(filter1);
    // vLayout->addWidget(filter2);



    connectUIElements();

    // Set default values of 5 for tesselation parameters
    onValChangeP1(5);
    onValChangeP2(5);

    // Set default values for near and far planes
    onValChangeNearBox(0.1f);
    onValChangeFarBox(10.f);
    // Set default DOF values
    onValChangeFocusDistBox(settings.focusDist);
    onValChangeFocusRangeBox(settings.focusRange);
    onValChangeMaxBlurRadiusBox(settings.maxBlurRadius);
    onValChangeIQIntensityBox(settings.rainforestIntensity);
}

void MainWindow::finish() {
    realtime->finish();
    delete(realtime);
}

void MainWindow::connectUIElements() {
    // From old Project 6
    //connectPerPixelFilter();
    //connectKernelBasedFilter();
    connectUploadFile();
    connectSaveImage();
    connectParam1();
    connectParam2();
    connectNear();
    connectFar();
    connectFocusDist();
    connectFocusRange();
    connectMaxBlurRadius();
    connect(fog, &QCheckBox::toggled, this, &MainWindow::onFogToggled);
    connectExtraCredit();
	connect(toggleScene, &QPushButton::clicked, this, &MainWindow::onToggleScene);
    // Rainforest intensity
    connect(iqIntensitySlider, &QSlider::valueChanged, this, &MainWindow::onValChangeIQIntensitySlider);
    connect(iqIntensityBox, static_cast<void(QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged),
            this, &MainWindow::onValChangeIQIntensityBox);
}


// From old Project 6
// void MainWindow::connectPerPixelFilter() {
//     connect(filter1, &QCheckBox::clicked, this, &MainWindow::onPerPixelFilter);
// }
// void MainWindow::connectKernelBasedFilter() {
//     connect(filter2, &QCheckBox::clicked, this, &MainWindow::onKernelBasedFilter);
// }

void MainWindow::connectExtraCredit() {
    connect(ec1, &QCheckBox::clicked, this, &MainWindow::onExtraCredit1);
    connect(ec2, &QCheckBox::clicked, this, &MainWindow::onExtraCredit2);
    connect(ec3, &QCheckBox::clicked, this, &MainWindow::onExtraCredit3);
    connect(ec4, &QCheckBox::clicked, this, &MainWindow::onExtraCredit4);
}

void MainWindow::connectUploadFile() {
    connect(uploadFile, &QPushButton::clicked, this, &MainWindow::onUploadFile);
}

void MainWindow::connectSaveImage() {
    connect(saveImage, &QPushButton::clicked, this, &MainWindow::onSaveImage);
}

void MainWindow::connectParam1() {
    connect(p1Slider, &QSlider::valueChanged, this, &MainWindow::onValChangeP1);
    connect(p1Box, static_cast<void(QSpinBox::*)(int)>(&QSpinBox::valueChanged),
            this, &MainWindow::onValChangeP1);
}

void MainWindow::connectParam2() {
    connect(p2Slider, &QSlider::valueChanged, this, &MainWindow::onValChangeP2);
    connect(p2Box, static_cast<void(QSpinBox::*)(int)>(&QSpinBox::valueChanged),
            this, &MainWindow::onValChangeP2);
}

void MainWindow::connectNear() {
    connect(nearSlider, &QSlider::valueChanged, this, &MainWindow::onValChangeNearSlider);
    connect(nearBox, static_cast<void(QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged),
            this, &MainWindow::onValChangeNearBox);
}

void MainWindow::connectFar() {
    connect(farSlider, &QSlider::valueChanged, this, &MainWindow::onValChangeFarSlider);
    connect(farBox, static_cast<void(QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged),
            this, &MainWindow::onValChangeFarBox);
}

void MainWindow::connectFocusDist() {
    connect(focusDistSlider, &QSlider::valueChanged, this, &MainWindow::onValChangeFocusDistSlider);
    connect(focusDistBox, static_cast<void(QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged),
            this, &MainWindow::onValChangeFocusDistBox);
}

void MainWindow::connectFocusRange() {
    connect(focusRangeSlider, &QSlider::valueChanged, this, &MainWindow::onValChangeFocusRangeSlider);
    connect(focusRangeBox, static_cast<void(QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged),
            this, &MainWindow::onValChangeFocusRangeBox);
}

void MainWindow::connectMaxBlurRadius() {
    connect(maxBlurRadiusSlider, &QSlider::valueChanged, this, &MainWindow::onValChangeMaxBlurRadiusSlider);
    connect(maxBlurRadiusBox, static_cast<void(QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged),
            this, &MainWindow::onValChangeMaxBlurRadiusBox);
}

// From old Project 6
// void MainWindow::onPerPixelFilter() {
//     settings.perPixelFilter = !settings.perPixelFilter;
//     realtime->settingsChanged();
// }
// void MainWindow::onKernelBasedFilter() {
//     settings.kernelBasedFilter = !settings.kernelBasedFilter;
//     realtime->settingsChanged();
// }

void MainWindow::onUploadFile() {
    // Get abs path of scene file
    QString configFilePath = QFileDialog::getOpenFileName(this, tr("Upload File"),
                                                          QDir::currentPath()
                                                              .append(QDir::separator())
                                                              .append("scenefiles")
                                                              .append(QDir::separator())
                                                              .append("realtime")
                                                              .append(QDir::separator())
                                                              .append("required"), tr("Scene Files (*.json)"));
    if (configFilePath.isNull()) {
        std::cout << "Failed to load null scenefile." << std::endl;
        return;
    }

    settings.sceneFilePath = configFilePath.toStdString();

    std::cout << "Loaded scenefile: \"" << configFilePath.toStdString() << "\"." << std::endl;

    realtime->sceneChanged();
}

void MainWindow::onSaveImage() {
    if (settings.sceneFilePath.empty()) {
        std::cout << "No scene file loaded." << std::endl;
        return;
    }
    std::string sceneName = settings.sceneFilePath.substr(0, settings.sceneFilePath.find_last_of("."));
    sceneName = sceneName.substr(sceneName.find_last_of("/")+1);
    QString filePath = QFileDialog::getSaveFileName(this, tr("Save Image"),
                                                    QDir::currentPath()
                                                        .append(QDir::separator())
                                                        .append("student_outputs")
                                                        .append(QDir::separator())
                                                        .append("realtime")
                                                        .append(QDir::separator())
                                                        .append("required")
                                                        .append(QDir::separator())
                                                        .append(sceneName), tr("Image Files (*.png)"));
    std::cout << "Saving image to: \"" << filePath.toStdString() << "\"." << std::endl;
    realtime->saveViewportImage(filePath.toStdString());
}

void MainWindow::onValChangeP1(int newValue) {
    p1Slider->setValue(newValue);
    p1Box->setValue(newValue);
    settings.shapeParameter1 = p1Slider->value();
    realtime->settingsChanged();
}

void MainWindow::onValChangeP2(int newValue) {
    p2Slider->setValue(newValue);
    p2Box->setValue(newValue);
    settings.shapeParameter2 = p2Slider->value();
    realtime->settingsChanged();
}

void MainWindow::onValChangeNearSlider(int newValue) {
    //nearSlider->setValue(newValue);
    nearBox->setValue(newValue/100.f);
    settings.nearPlane = nearBox->value();
    realtime->settingsChanged();
}

void MainWindow::onValChangeFarSlider(int newValue) {
    //farSlider->setValue(newValue);
    farBox->setValue(newValue/100.f);
    settings.farPlane = farBox->value();
    realtime->settingsChanged();
}

void MainWindow::onValChangeNearBox(double newValue) {
    nearSlider->setValue(int(newValue*100.f));
    //nearBox->setValue(newValue);
    settings.nearPlane = nearBox->value();
    realtime->settingsChanged();
}

void MainWindow::onValChangeFarBox(double newValue) {
    farSlider->setValue(int(newValue*100.f));
    //farBox->setValue(newValue);
    settings.farPlane = farBox->value();
    realtime->settingsChanged();
}

void MainWindow::onValChangeFocusDistSlider(int newValue) {
    focusDistBox->setValue(newValue / 100.f);
    settings.focusDist = focusDistBox->value();
    realtime->settingsChanged();
}

void MainWindow::onValChangeFocusDistBox(double newValue) {
    focusDistSlider->setValue(int(newValue * 100.f));
    settings.focusDist = focusDistBox->value();
    realtime->settingsChanged();
}

void MainWindow::onValChangeFocusRangeSlider(int newValue) {
    focusRangeBox->setValue(newValue / 100.f);
    settings.focusRange = focusRangeBox->value();
    realtime->settingsChanged();
}

void MainWindow::onValChangeFocusRangeBox(double newValue) {
    focusRangeSlider->setValue(int(newValue * 100.f));
    settings.focusRange = focusRangeBox->value();
    realtime->settingsChanged();
}

void MainWindow::onValChangeMaxBlurRadiusSlider(int newValue) {
    maxBlurRadiusBox->setValue(newValue / 100.f);
    settings.maxBlurRadius = maxBlurRadiusBox->value();
    realtime->settingsChanged();
}

void MainWindow::onValChangeMaxBlurRadiusBox(double newValue) {
    maxBlurRadiusSlider->setValue(int(newValue * 100.f));
    settings.maxBlurRadius = maxBlurRadiusBox->value();
    realtime->settingsChanged();
}

// Extra Credit:

void MainWindow::onExtraCredit1() {
    settings.extraCredit1 = !settings.extraCredit1;
    realtime->settingsChanged();
}

void MainWindow::onExtraCredit2() {
    settings.extraCredit2 = !settings.extraCredit2;
    realtime->settingsChanged();
}

void MainWindow::onExtraCredit3() {
    settings.extraCredit3 = !settings.extraCredit3;
    realtime->settingsChanged();
}

void MainWindow::onExtraCredit4() {
    settings.extraCredit4 = !settings.extraCredit4;
    realtime->settingsChanged();
}

void MainWindow::onFogToggled(bool checked) {
    settings.fogEnabled = checked;
    realtime->settingsChanged();
}

void MainWindow::onToggleScene() {
	// Toggle between IQ and Water
	if (settings.fullscreenScene == FullscreenScene::IQ) {
		settings.fullscreenScene = FullscreenScene::Water;
		toggleScene->setText(QStringLiteral("Scene: Water"));
	} else {
		settings.fullscreenScene = FullscreenScene::IQ;
		toggleScene->setText(QStringLiteral("Scene: Rainforest"));
	}
	realtime->settingsChanged();
}

void MainWindow::onValChangeIQIntensitySlider(int newValue) {
    iqIntensityBox->setValue(newValue / 100.0);
    settings.rainforestIntensity = iqIntensityBox->value();
    realtime->settingsChanged();
}

void MainWindow::onValChangeIQIntensityBox(double newValue) {
    iqIntensitySlider->setValue(int(newValue * 100.0));
    settings.rainforestIntensity = iqIntensityBox->value();
    realtime->settingsChanged();
}
