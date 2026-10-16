// The one place that reads Squirrel 2.2.4 internals (we build the VM ourselves): a closure's source name and line table.
// Kept in its own translation unit because the VM's private headers define macros (type(), ...) that clash with others.

#include "../../third_party/squirrel-2.2.4/squirrel/sqpcheader.h"
#include "../../third_party/squirrel-2.2.4/squirrel/sqvm.h"
#include "../../third_party/squirrel-2.2.4/squirrel/sqstring.h"
#include "../../third_party/squirrel-2.2.4/squirrel/sqfuncproto.h"
#include "../../third_party/squirrel-2.2.4/squirrel/sqclosure.h"

#include <string>

namespace sqedit {

bool SqClosureSourceRange(HSQUIRRELVM v, SQInteger idx, std::string* source, int* first, int* last)
{
	HSQOBJECT o;
	sq_getstackobj(v, idx, &o);
	if (o._type != OT_CLOSURE) return false;
	SQFunctionProto* f = _funcproto(o._unVal.pClosure->_function);
	if (type(f->_sourcename) == OT_STRING) *source = _stringval(f->_sourcename);
	*first = *last = 0;
	for (SQInteger i = 0; i < f->_nlineinfos; ++i) {
		const int l = (int)f->_lineinfos[i]._line;
		if (!*first || l < *first) *first = l;
		if (l > *last) *last = l;
	}
	return true;
}

} // namespace sqedit
