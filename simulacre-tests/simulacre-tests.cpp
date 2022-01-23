#include "CppUnitTest.h"

#include "simulacre.h"

#include <iostream>

#include "dbghelp.h"

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




struct ObjectToMock {
	int simpleFunctionToMockWithNearRelativeCall() {
		return foo();
	}
};




struct ObjectToMockWithVirtualMethod {
	virtual int simpleVirtualFunctionToMockWithSamePrefix() {
		return foo();
	}

	virtual int simpleVirtualFunctionToMock() {
		return foo();
	}

	virtual int otherSimpleVirtualFunctionToMock() {
		return bar();
	}
};




namespace simulacretests
{
	TEST_CLASS(simulacretests)
	{
	public:

		TEST_METHOD(mockFunction_simpleFunctionToMockWithNearRelativeCall)
		{
			Simulacre simulacre("simulacre-tests.dll");
			Assert::AreEqual(simulacre.mock(&simpleFunctionToMockWithNearRelativeCall, &foo, &bar), S_OK);
			Assert::AreEqual(simpleFunctionToMockWithNearRelativeCall(), 0xba4);
		}


		TEST_METHOD(mockFunction_simpleFunctionToMockWithNearAbsoluteIndirectCall)
		{
			Simulacre simulacre("simulacre-tests.dll");
			Assert::AreEqual(simulacre.mock(&simpleFunctionToMockWithNearAbsoluteIndirectCall, &malloc, &mallocMock), S_OK);
			Assert::AreEqual(simpleFunctionToMockWithNearAbsoluteIndirectCall(), (void*)0xdeadbeef);
		}


		TEST_METHOD(mockFunction_notExistingFunction)
		{
			Simulacre simulacre("simulacre-tests.dll");
			Assert::AreEqual(simulacre.mock((void*)0x12345678, &foo, &bar), E_FAIL);
		}


		TEST_METHOD(mockFunction_object_simpleFunctionToMockWithNearAbsoluteIndirectCall)
		{
			Simulacre simulacre("simulacre-tests.dll");

			ObjectToMock objectToMock;
			Assert::AreEqual(simulacre.mock(Simulacre::getMemberFunctionAddress(&ObjectToMock::simpleFunctionToMockWithNearRelativeCall), &foo, &bar), S_OK);
			Assert::AreEqual(objectToMock.simpleFunctionToMockWithNearRelativeCall(), 0xba4);
		}


		TEST_METHOD(mockVirtualMethod_simpleVirtualFunctionToMock)
		{
			Simulacre simulacre("simulacre-tests.dll");

			ObjectToMockWithVirtualMethod object;
			Assert::AreEqual(simulacre.mockVirtualMethod("ObjectToMockWithVirtualMethod::simpleVirtualFunctionToMock", &foo, &bar), S_OK);
			Assert::AreEqual(object.simpleVirtualFunctionToMock(), 0xba4);
		}


		TEST_METHOD(mockVirtualMethod_notExistingMethod)
		{
			Simulacre simulacre("simulacre-tests.dll");

			ObjectToMockWithVirtualMethod object;
			Assert::AreEqual(simulacre.mockVirtualMethod("ObjectToMockWithVirtualMethod::notExistingMethod", &foo, &bar), E_FAIL);
		}
	};
}
