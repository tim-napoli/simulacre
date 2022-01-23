#include "CppUnitTest.h"

#include "cppmock.h"

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


		TEST_METHOD(mockFunction_object_simpleFunctionToMockWithNearAbsoluteIndirectCall)
		{
			CPPMock cppMock("cppmock-tests.dll");

			ObjectToMock objectToMock;
			Assert::AreEqual(cppMock.mockFunction(CPPMock::getMemberFunctionAddress(&ObjectToMock::simpleFunctionToMockWithNearRelativeCall), &foo, &bar), S_OK);
			Assert::AreEqual(objectToMock.simpleFunctionToMockWithNearRelativeCall(), 0xba4);
		}


		TEST_METHOD(mockVirtualMethod_simpleVirtualFunctionToMock)
		{
			CPPMock cppMock("cppmock-tests.dll");

			ObjectToMockWithVirtualMethod object;
			Assert::AreEqual(cppMock.mockVirtualMethod(&object, "simpleVirtualFunctionToMock", &foo, &bar), S_OK);
			Assert::AreEqual(object.simpleVirtualFunctionToMock(), 0xba4);
		}


		TEST_METHOD(mockVirtualMethod_notExistingMethod)
		{
			CPPMock cppMock("cppmock-tests.dll");

			ObjectToMockWithVirtualMethod object;
			Assert::AreEqual(cppMock.mockVirtualMethod(&object, "notExistingMethod", &foo, &bar), E_FAIL);
		}
	};
}
