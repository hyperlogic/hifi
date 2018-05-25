//
//  AutoTester.cpp
//  zone/ambientLightInheritence
//
//  Created by Nissim Hadar on 2 Nov 2017.
//  Copyright 2013 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//
#include "AutoTester.h"

AutoTester::AutoTester(QWidget *parent) : QMainWindow(parent) {
    ui.setupUi(this);
    ui.checkBoxInteractiveMode->setChecked(true);
    ui.progressBar->setVisible(false);

    signalMapper = new QSignalMapper();

    connect(ui.actionClose, &QAction::triggered, this, &AutoTester::on_closeButton_clicked);
    connect(ui.actionAbout, &QAction::triggered, this, &AutoTester::about);

    test = new Test();
}

void AutoTester::runFromCommandLine(const QString& testFolder) {
    isRunningFromCommandline = true;
    test->startTestsEvaluation(testFolder);
}

void AutoTester::on_evaluateTestsButton_clicked() {
    test->startTestsEvaluation();
}

void AutoTester::on_createRecursiveScriptButton_clicked() {
    test->createRecursiveScript();
}

void AutoTester::on_createAllRecursiveScriptsButton_clicked() {
    test->createAllRecursiveScripts();
}

void AutoTester::on_createTestsButton_clicked() {
	test->createTests();
}

void AutoTester::on_createMDFileButton_clicked() {
    test->createMDFile();
}

void AutoTester::on_createAllMDFilesButton_clicked() {
    test->createAllMDFiles();
}

void AutoTester::on_createTestsOutlineButton_clicked() {
    test->createTestsOutline();
}

void AutoTester::on_closeButton_clicked() {
    exit(0);
}

void AutoTester::downloadImage(const QUrl& url) {
    downloaders.emplace_back(new Downloader(url, this));
    connect(downloaders[_index], SIGNAL (downloaded()), signalMapper, SLOT (map()));

    signalMapper->setMapping(downloaders[_index], _index);

    ++_index;
}

void AutoTester::downloadImages(const QStringList& URLs, const QString& directoryName, const QStringList& filenames) {
    _directoryName = directoryName;
    _filenames = filenames;

    _numberOfImagesToDownload = URLs.size();
    _numberOfImagesDownloaded = 0;
    _index = 0;

    ui.progressBar->setMinimum(0);
    ui.progressBar->setMaximum(_numberOfImagesToDownload - 1);
    ui.progressBar->setValue(0);
    ui.progressBar->setVisible(true);

    for (int i = 0; i < _numberOfImagesToDownload; ++i) {
        QUrl imageURL(URLs[i]);
        downloadImage(imageURL);
    }

    connect(signalMapper, SIGNAL (mapped(int)), this, SLOT (saveImage(int)));
}

void AutoTester::saveImage(int index) {
    QPixmap pixmap;
    pixmap.loadFromData(downloaders[index]->downloadedData());

    QImage image = pixmap.toImage();
    image = image.convertToFormat(QImage::Format_RGB32);

    QString fullPathname = _directoryName + "/" + _filenames[index];
    if (!image.save(fullPathname, 0, 100)) {
        QMessageBox::information(0, "Test Aborted", "Failed to save image: " + _filenames[index]);
        ui.progressBar->setVisible(false);
        return;
    }

    ++_numberOfImagesDownloaded;

    if (_numberOfImagesDownloaded == _numberOfImagesToDownload) {
        test->finishTestsEvaluation(isRunningFromCommandline, ui.checkBoxInteractiveMode->isChecked(), ui.progressBar);
    } else {
        ui.progressBar->setValue(_numberOfImagesDownloaded);
    }
}

void AutoTester::about() {
    QMessageBox::information(0, "About", QString("Built ") + __DATE__ + " : " + __TIME__);
}
