#pragma once
#include <windows.h>
#include <dbghelp.h>

#include <vector>




/* CPPMock
 *
 * Objet permettant de mocker des fonctions CPPs.
 */
struct CPPMock
{
   /* Constructor
    *
    * Use `_sModuleName` to specify the DLL you want to mock (useful for mocking a test DLL).
    * If `_sModuleName` is not specified, the symbols are loaded from the executable.
    */
   CPPMock(const std::string& _sModuleName = "");


   /* Destructor
    *
    */
   ~CPPMock();


   /* getSymbolAddress()
    *
    * Retourne l'adresse dans le fichier de symboles (PDB) du symbole donnée
    * en paramètres dont l'adresse est telle que le symbole est chargé en mémoire.
    * Retourne `nullptr` si le symbole ne peut pas être trouvé.
    */
   ULONG64 getSymbolAddress(void* _pFunctionAddress);


   /* getSymbol()
    *
    * Retourne le symbole du fichier de symbol du symbole dont l'adresse en mémoire
    * une fois chargé est donnée en paramètre.
    * Retourne `nullptr` si le symbole ne peut être trouvé.
    * Si le symbole est trouvé, l'appelant doit libérer de lui même le SYMBOL_INFO* retourné.
    */
   SYMBOL_INFO* getSymbol(void* _pFunctionAddress);


   /* rewriteFunctionCodeCalls()
    *
    * Réécrit les appels à `_pOldFunctionAddress` en appels à `_pNewFunctionAddress` dans le code de la fonction
    * situé à `_pFunctionAddress` et de taille `_sizeFunctionSize`.
    * Retourne `S_OK` si les appels ont effectivement été remplacés dans le code la fonction, ou `E_FAIL` si une
    * erreur est survenue.
    */
   HRESULT rewriteFunctionCodeCalls(void* _pFunctionAddress, size_t _sizeFunctionSize, void* _pOldFunctionAddress, void* _pNewFunctionAddress);


   /* mockFunction()
    *
    * Remplace dans la fonction `_pFunctionAddress` les appels à `_pOldFunctionAddress` par des
    * appels à `_pNewFunctionAddress`.
    * Retourne `S_OK` si les appels ont effectivement été remplacés, ou `E_FAIL` dans le cas
    * contraire.
    */
   HRESULT mockFunction(void* _pFunctionAddress, void* _pOldFunctionAddress, void* _pNewFunctionAddress);


   /* getVirtualMethodAddressAndSize()
    *
    * Retourne l'adresse de la méthode virtuelle donnée dans la mémoire du processus.
    * Si la méthode virtuelle est trouvée, une valeur différente de `nullptr` est renvoyée et
    * la taille du code de la méthode est renseigné dans `_sizeVirtualMethodeSize`.
    */
   void* getVirtualMethodAddress(void* _pObjectInstance, const std::string& _sVirtualMethodeName, size_t& _sizeVirtualMethodeSize);


   /* mockVirtualMethod()
    *
    * Même fonctionnement que `mockFunction()`, mais pour une méthode virtuelle.
    */
   HRESULT mockVirtualMethod(void* _pObjectInstance, const std::string& _sVirtualMethodName, void* _pOldFunctionAddress, void* _pNewFunctionAddress);


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