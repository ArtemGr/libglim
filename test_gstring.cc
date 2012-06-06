
#include "gstring.hpp"
using glim::gstring;
#include <assert.h>
#include <stdlib.h>
#include <iostream>
#include <string>
#include <stdexcept>
#include <sstream>
#include <unordered_map>

int main () {
  std::cout << "Testing gstring.hpp ... " << std::flush;

  gstring gs;
  if (gs.needsFreeing()) throw std::runtime_error ("Default gstring needsFreeing");
  if (gs.isReadOnly()) throw std::runtime_error ("Default gstring isReadOnly");
  if (gs.capacity() != 1) throw std::runtime_error ("Default gstring capacity is not 1");
  char buf16[16];
  gstring gs16 (sizeof (buf16), buf16, false, false, 0);
  if (gs16.capacity() != 16) throw std::runtime_error ("gs16 capacity != 16");
  if (gs16.size() != 0) throw std::runtime_error ("gs16 size != 0");
  gstring gsFree (17, NULL, true, false, 0);
  if (!gsFree.needsFreeing()) throw std::runtime_error ("!needsFreeing");
  if (gsFree.isReadOnly()) throw std::runtime_error ("isReadOnly");
  if (gsFree.capacity() != 16) throw std::runtime_error ("gsFree capacity != 16");
  if (gsFree.size() != 0) throw std::runtime_error ("gsFree size != 0");
  gstring gsRO (0, NULL, false, true, 0);
  if (gsRO.needsFreeing()) throw std::runtime_error ("needsFreeing");
  if (!gsRO.isReadOnly()) throw std::runtime_error ("!isReadOnly");
  if (gsRO.capacity() != 1) throw std::runtime_error ("gsRO capacity != 1");
  if (gsRO.size() != 0) throw std::runtime_error ("gsRO size != 0");
  char buf32[32];
  gstring gs32 (sizeof (buf32), buf32, false, false, 0);
  if (gs32.capacity() != 32) throw std::runtime_error ("capacity != 32");
  if (gs32.size() != 0) throw std::runtime_error ("gs32 size != 0");
  const gstring foo = C2GSTRING ("foo");
  if (foo.needsFreeing()) throw std::runtime_error ("foo needsFreeing");
  if (!foo.isReadOnly()) throw std::runtime_error ("foo !isReadOnly");
  if (foo != "foo") throw std::runtime_error ("foo != foo");
  if (foo.size() != 3) throw std::runtime_error ("foo not 3");
  std::ostringstream oss; oss << gs16 << gsFree << gsRO << gs32 << foo;
  if (oss.str() != "foo") throw std::runtime_error ("oss foo != foo");
  glim::gstring_stream gss (gs16); std::ostream gsos (&gss);
  gsos << "bar" << std::flush;
  if (gs16 != "bar") throw std::runtime_error ("gs16 != bar");
  gsos << "beer" << std::flush;
  if (gs16 != "barbeer") throw std::runtime_error ("gs16 != barbeer");
  gsos << "123456789" << std::flush;
  if (gs16 != "barbeer123456789") throw std::runtime_error ("gs16 != barbeer123456789");
  if (gs16.capacity() != 16) throw std::runtime_error ("gs16 != 16");
  gsos << '0' << std::flush;
  if (gs16 != "barbeer1234567890") throw std::runtime_error ("gs16 != barbeer1234567890");
  if (gs16.capacity() != 32) throw std::runtime_error ("gs16 != 32");

  gstring gsb; std::string str ("abc");
  gsb << 'a' << 1 << 2LL << str;
  std::string ns ("1:3,"); std::istringstream nsi (ns);
  gsb.readNetstring (nsi);
  if (gsb != "a12abc3") throw std::runtime_error ("gsb != a12abc3");
  if (strcmp (gsb.c_str(), "a12abc3") != 0) throw std::runtime_error ("strcmp ! 0");

  gsb.clear().appendNetstring ("foo") .appendNetstring ("bar");
  if (gsb != "3:foo,3:bar,") throw std::runtime_error ("gsb != 3:foo,3:bar,");
  uint32_t pos = 0;
  if (gsb.netstringAt (pos, &pos) != "foo" || gsb.netstringAt (pos, &pos) != "bar" || pos != gsb.length())
    throw std::runtime_error ("gsb !netstringAt");

  gs32.clear() << 12345 << ',';
  if (gs32.intAt (0, &pos) != 12345 || pos != 5) throw std::runtime_error ("gsb !12345");
  if (gs32.intAt (1, &pos) != 2345 || pos != 5) throw std::runtime_error ("gsb !2345");
  if (gs32.intAt (5, &pos) != 0 || pos != 5) throw std::runtime_error ("gsb !0");

  std::unordered_map<glim::gstring, int> map;
  map[glim::gstring ("foo")] = 1;
  glim::gstring bar ("bar");
  map[bar] = 1;
  map[glim::gstring ("sum")] = map[glim::gstring ("foo")] + map[glim::gstring ("bar")];
  if (map[glim::gstring ("sum")] != 2) throw std::runtime_error ("sum != 2");
  map.clear();

  std::cout << "pass." << std::endl;
  return 0;
}
