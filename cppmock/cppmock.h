#pragma once
#include <windows.h>

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


   /* getFunctionSymbolAddress()
    *
    * Retourne l'adresse dans le fichier de symboles (PDB) de la fonction donnée
    * en paramètres.
    * Retourne `nullptr` si le symbole ne peut pas être trouvé.
    */
   ULONG64 getFunctionSymbolAddress(void* _pFunctionAddress);


   /* mockFunction()
    *
    * Remplace dans la fonction `_pFunctionAddress` les appels à `_pOldFunctionAddress` par des
    * appels à `_pNewFunctionAddress`.
    * Retourne `S_OK` si les appels ont effectivement été remplacés, ou `E_FAIL` dans le cas
    * contraire.
    * 
    * REMARQUES
    * Pour l'instant, seules les instructions call relatifs (opcode 0x86) sont supportées.
    *
    * TODO
    * Supporter les instructions calls absolus (0xff, 0x9a).
    */
   HRESULT mockFunction(void* _pFunctionAddress, void* _pOldFunctionAddress, void* _pNewFunctionAddress);


   std::string m_sModuleName;
   HANDLE m_hProcess;
   DWORD  m_dwProcessBaseAddress;
   std::vector<uint32_t> m_vui32CallIndirectTable;
};