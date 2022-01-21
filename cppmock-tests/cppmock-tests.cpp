#include "CppUnitTest.h"

#include "cppmock.h"

#include <iostream>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;


int foo() {
	return 0xf00;
}


int bar() {
	return 0xba4;
}


int simpleFunctionToMockWithNearRelativeCall() {
	return foo();
}


void* mallocMock(size_t _size) {
	return (void*)0xdeadbeef;
}


void* simpleFunctionToMockWithNearAbsoluteIndirectCall() {
	return malloc(32);
}


namespace cppmocktests
{
	TEST_CLASS(cppmocktests)
	{
	public:
		
		TEST_METHOD(mockFunction_simpleFunctionToMockWithNearRelativeCall)
		{
			CPPMock cppMock("cppmock-tests.dll");
			Assert::AreEqual(cppMock.mockFunction(&simpleFunctionToMockWithNearRelativeCall, &foo, &bar), S_OK);
			Assert::AreEqual(simpleFunctionToMockWithNearRelativeCall(), 0xba4);
		}

		TEST_METHOD(mockFunction_simpleFunctionToMockWithNearAbsoluteIndirectCall)
		{
			CPPMock cppMock("cppmock-tests.dll");
			Assert::AreEqual(cppMock.mockFunction(&simpleFunctionToMockWithNearAbsoluteIndirectCall, &malloc, &mallocMock), S_OK);
			Assert::AreEqual(simpleFunctionToMockWithNearAbsoluteIndirectCall(), (void*)0xdeadbeef);
		}

		TEST_METHOD(mockFunction_notExistingFunction)
		{
			CPPMock cppMock("cppmock-tests.dll");
			Assert::AreEqual(cppMock.mockFunction((void*)0x12345678, &foo, &bar), E_FAIL);
		}
	};
}
