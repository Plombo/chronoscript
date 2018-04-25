#include "test/expect.h"

void main()
{
    set_global_variant("foo", 5);
    expect(get_global_variant("foo"), 5);
    set_global_variant("foo", "foo_value1");
    expect(get_global_variant("foo"), "foo_value1");
    set_global_variant("foo", "foo_value2");
    expect(get_global_variant("foo"), "foo_value2");
}

