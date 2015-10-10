//
//  SmoothOperatorTests.h
//  tests/shared/src
//
//  Created by Anthony J. Thibault on 9/10/2015
//  Copyright 2015 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_SmoothOperatorTests_h
#define hifi_SmoothOperatorTests_h

#include <QtTest/QtTest>

class SmoothOperatorTests : public QObject {
    Q_OBJECT
private slots:
    void testInterpolation();
};

#endif // hifi_SmoothOperatorTests_h
