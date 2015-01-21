#define SLIST_HEAD(name, type)			\
struct name {					\
		struct type *slh_first;		\
}

typedef SLIST_HEAD(, Pkglist) Plisthead;

Plisthead *
order_install(Plisthead *impacthead)
{
	__coverity_alloc_nosize__();
}
