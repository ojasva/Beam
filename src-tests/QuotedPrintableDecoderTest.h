/*
 * Copyright 2002-2006, project beam (http://sourceforge.net/projects/beam).
 * All rights reserved. Distributed under the terms of the GNU GPL v2.
 *
 * Authors:
 *		Oliver Tappe <beam@hirschkaefer.de>
 */
/*
 * Beam's test-application is based on the OpenBeOS testing framework
 * (which in turn is based on cppunit). Big thanks to everyone involved!
 *
 */


#ifndef _QuotedPrintableDecoderTest_h
#define _QuotedPrintableDecoderTest_h

#include <cppunit/TestCaller.h>
#include <cppunit/TestSuite.h>
#include <cppunit/extensions/HelperMacros.h>
#include <TestCase.h>

class QuotedPrintableDecoderTest : public BTestCase
{
	typedef TestCase inherited;
	CPPUNIT_TEST_SUITE( QuotedPrintableDecoderTest );
	CPPUNIT_TEST( SimpleTest);
	CPPUNIT_TEST( MultiLineTest);
	CPPUNIT_TEST( LargeDataTest);
	CPPUNIT_TEST_SUITE_END();
public:
//	static CppUnit::Test* Suite();
	
	// This function called before *each* test added in Suite()
	void setUp();
	
	// This function called after *each* test added in Suite()
	void tearDown();

	//------------------------------------------------------------
	// Test functions
	//------------------------------------------------------------
	void SimpleTest();
	void MultiLineTest();
	void LargeDataTest();
};


#endif
