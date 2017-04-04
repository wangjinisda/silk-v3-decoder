/* add_basic.c

   Demonstrates creating a DLL with an exported function, the inflexible way.
*/

__declspec(dllexport) int __cdecl Add(int a, int b)
{
  return (a + b);
}