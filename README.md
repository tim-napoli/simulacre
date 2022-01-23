# Simulacre

Simulacre is a C++ library intended to allow to mock functions in order to write unit tests for Win32 projects.


## How Does it work ?

Simulacre relies on a simple principle : let say you have a function `functionToMock()` that need to be mocked by replacing the underlying function `foo()` by a function `bar()`.
Simulacre will read the code of `functionToMock()`, detecting instructions calls to `foo()` and replacing them with calls to `bar()`.


## Example

```
void foo() {
	std::cout << "foo" << std::endl;
}

void bar() {
	std::cout << "bar" << std::endl;
}

void functionToMock() {
	foo();
}

int main(int argc, char** argv) {
	Simulacre simulacre;
	simulacre.mockFunction(&functionToMock, &foo, &bar);

	functionToMock();
	return 0;
}
```

This sample will print when executed `bar` instead of `foo`.


## Limitations

Given Simulacre will only replace calls of `foo()` in the assembly code of the function to mock, it will not change any other occurences of calls to `foo()` in other functions implicitly.

In order to make Simulacre working, you need to disable incremental linking in the code you are testing (`/INCREMENTAL:no`). If you plans to test Release code, you need to disable inlining of functions (`/Ob0`).

Simulacre only works with Visual Studio compiler and x86 code.