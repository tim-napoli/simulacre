#pragma once
#include <windows.h>
#include <dbghelp.h>

#include <vector>




/* Simulacre
 *
 * Object allowing to mock C++ methods by replacing address of called functions in
 * arbitrary functions.
 */
struct Simulacre
{
   /* Constructor
    *
    * Use `_sModuleName` to specify the DLL you want to mock (useful for mocking a test DLL).
    * If `_sModuleName` is not specified, the symbols are loaded from the executable.
    */
   Simulacre(const std::string& _sModuleName = "");


   /* Destructor
    *
    */
   ~Simulacre();


   /* getSymbolAddress()
    *
    * Returns the virtual address of the symbol located at `_pProcessAddress`.
    * `_pProcessAddress` is the address of the symbol once loaded into memory.
    * Returns `0` if the symbol cannot be found.
    */
   ULONG64 getSymbolAddress(void* _pProcessAddress);


   /* getSymbolAddress()
    *
    * Returns the process address of the symbol having virtual address `_ul64VirtualAddress`.
    */
   void* symbolToProcessAddress(ULONG64 _ul64VirtualAddress);


   /* getSymbol()
    *
    * Returns the symbol information of the symbol located at `_pProcessAddress`
    * in the process memory.
    * If the symbol can't be found, the method returns `nullptr`.
    */
   SYMBOL_INFO* getSymbol(void* _pFunctionAddress);


   /* getSymbolName()
    *
    * Returns the symbol information of the symbol called `_sSymbolName`.
    * If the symbol can't be found, the method returns `nullptr`.
    */
   SYMBOL_INFO* getSymbolFromName(const std::string& _sSymbolName);


   /* replaceFunctionCalls()
    *
    * Rewrite into the code of function located at `_pFunctionAddress` of size `_sizeFunctionSize`
    * every calls to `_pOldFunctionAddress` to calls to function `_pNewFunctionAddress`.
    * Returns `S_OK` in case of success, or `E_FAIL` otherwise.
    */
   HRESULT replaceFunctionCalls(void* _pFunctionAddress, size_t _sizeFunctionSize,
                                void* _pOldFunctionAddress, void* _pNewFunctionAddress);


   /* mock()
    *
    * Helper around `replaceFunctionCalls()`. Just need to give the address of the function in which
    * we want to replace calls instead of the address and the size. The size is retrieved from
    * symbols file.
    */
   HRESULT mock(void* _pFunctionAddress, void* _pOldFunctionAddress, void* _pNewFunctionAddress);


   /* getVirtualMethodAddressAndSize()
    *
    * Returns the address in process memory of the virtual method named `_sVirtualMethodeName`.
    * Returns `nullptr` if the method cannot be found. Otherwise, parameter `_sizeVirtualMethodeSize`
    * is set with the code size of the method.
    */
   void* getVirtualMethodAddress(const std::string& _sVirtualMethodeName,
                                 size_t& _sizeVirtualMethodeSize);


   /* mockVirtualMethod()
    *
    * Works like `mock()` but is applied on a virtual method of some object.
    * Returns `S_OK` if the code has been successfuly patched, or `E_FAIL` otherwise.
    *
    * REMARK
    * In release build, it could happen that a virtual method has the same code than an existing
    * method. Perhaps we should add some warning if a patched symbol has multiple references inside
    * the symbols file ?
    */
   HRESULT mockVirtualMethod(const std::string& _sVirtualMethodName,
                             void* _pOldFunctionAddress, void* _pNewFunctionAddress);


   /* getMemberFunctionAddress()
    *
    * Retourne l'adresse de la méthode donnée en paramètre.
    */
   template<typename FunctionType>
   static void* getMemberFunctionAddress(FunctionType _function) {
      union {
         FunctionType function;
         void* functionAddress;
      } u = { _function };
      return u.functionAddress;
   }


   std::string m_sModuleName;
   HANDLE m_hProcess;
   DWORD  m_dwProcessBaseAddress;
   std::vector<uint32_t> m_vui32CallIndirectTable;
};