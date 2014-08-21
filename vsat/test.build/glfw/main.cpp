
#include "main.h"

//${CONFIG_BEGIN}
#define CFG_BINARY_FILES *.bin|*.dat
#define CFG_BRL_GAMETARGET_IMPLEMENTED 1
#define CFG_BRL_THREAD_IMPLEMENTED 1
#define CFG_CONFIG release
#define CFG_CPP_GC_MODE 1
#define CFG_GLFW_SWAP_INTERVAL -1
#define CFG_GLFW_USE_MINGW 1
#define CFG_GLFW_WINDOW_FULLSCREEN 0
#define CFG_GLFW_WINDOW_HEIGHT 480
#define CFG_GLFW_WINDOW_RESIZABLE 0
#define CFG_GLFW_WINDOW_TITLE Monkey Game
#define CFG_GLFW_WINDOW_WIDTH 640
#define CFG_HOST macos
#define CFG_IMAGE_FILES *.png|*.jpg
#define CFG_LANG cpp
#define CFG_MOJO_AUTO_SUSPEND_ENABLED 1
#define CFG_MOJO_DRIVER_IMPLEMENTED 1
#define CFG_MOJO_IMAGE_FILTERING_ENABLED 1
#define CFG_MUSIC_FILES *.wav|*.ogg
#define CFG_OPENGL_DEPTH_BUFFER_ENABLED 0
#define CFG_OPENGL_GLES20_ENABLED 0
#define CFG_REFLECTION_FILTER foundation.vec2;monkey*;coregfx.color;framework.entity;framework.sprite;framework.shapes
#define CFG_SAFEMODE 0
#define CFG_SOUND_FILES *.wav|*.ogg
#define CFG_TARGET glfw
#define CFG_TEXT_FILES *.xml|*.fnt
#define CFG_TRACK_UNLOCALIZED_WORDS 0
//${CONFIG_END}

//${TRANSCODE_BEGIN}

#include <wctype.h>
#include <locale.h>

// C++ Monkey runtime.
//
// Placed into the public domain 24/02/2011.
// No warranty implied; use at your own risk.

//***** Monkey Types *****

typedef wchar_t Char;
template<class T> class Array;
class String;
class Object;

#if CFG_CPP_DOUBLE_PRECISION_FLOATS
typedef double Float;
#define FLOAT(X) X
#else
typedef float Float;
#define FLOAT(X) X##f
#endif

void dbg_error( const char *p );

#if !_MSC_VER
#define sprintf_s sprintf
#define sscanf_s sscanf
#endif

//***** GC Config *****

#define DEBUG_GC 0

// GC mode:
//
// 0 = disabled
// 1 = Incremental GC every OnWhatever
// 2 = Incremental GC every allocation
//
#ifndef CFG_CPP_GC_MODE
#define CFG_CPP_GC_MODE 1
#endif

//How many bytes alloced to trigger GC
//
#ifndef CFG_CPP_GC_TRIGGER
#define CFG_CPP_GC_TRIGGER 8*1024*1024
#endif

//GC_MODE 2 needs to track locals on a stack - this may need to be bumped if your app uses a LOT of locals, eg: is heavily recursive...
//
#ifndef CFG_CPP_GC_MAX_LOCALS
#define CFG_CPP_GC_MAX_LOCALS 8192
#endif

// ***** GC *****

#if _WIN32

int gc_micros(){
	static int f;
	static LARGE_INTEGER pcf;
	if( !f ){
		if( QueryPerformanceFrequency( &pcf ) && pcf.QuadPart>=1000000L ){
			pcf.QuadPart/=1000000L;
			f=1;
		}else{
			f=-1;
		}
	}
	if( f>0 ){
		LARGE_INTEGER pc;
		if( QueryPerformanceCounter( &pc ) ) return pc.QuadPart/pcf.QuadPart;
		f=-1;
	}
	return 0;// timeGetTime()*1000;
}

#elif __APPLE__

#include <mach/mach_time.h>

int gc_micros(){
	static int f;
	static mach_timebase_info_data_t timeInfo;
	if( !f ){
		mach_timebase_info( &timeInfo );
		timeInfo.denom*=1000L;
		f=1;
	}
	return mach_absolute_time()*timeInfo.numer/timeInfo.denom;
}

#else

int gc_micros(){
	return 0;
}

#endif

#define gc_mark_roots gc_mark

void gc_mark_roots();

struct gc_object;

gc_object *gc_object_alloc( int size );
void gc_object_free( gc_object *p );

struct gc_object{
	gc_object *succ;
	gc_object *pred;
	int flags;
	
	virtual ~gc_object(){
	}
	
	virtual void mark(){
	}
	
	void *operator new( size_t size ){
		return gc_object_alloc( size );
	}
	
	void operator delete( void *p ){
		gc_object_free( (gc_object*)p );
	}
};

gc_object gc_free_list;
gc_object gc_marked_list;
gc_object gc_unmarked_list;
gc_object gc_queued_list;	//doesn't really need to be doubly linked...

int gc_free_bytes;
int gc_marked_bytes;
int gc_alloced_bytes;
int gc_max_alloced_bytes;
int gc_new_bytes;
int gc_markbit=1;

gc_object *gc_cache[8];

int gc_ctor_nest;
gc_object *gc_locals[CFG_CPP_GC_MAX_LOCALS],**gc_locals_sp=gc_locals;

void gc_collect_all();
void gc_mark_queued( int n );

#define GC_CLEAR_LIST( LIST ) ((LIST).succ=(LIST).pred=&(LIST))

#define GC_LIST_IS_EMPTY( LIST ) ((LIST).succ==&(LIST))

#define GC_REMOVE_NODE( NODE ){\
(NODE)->pred->succ=(NODE)->succ;\
(NODE)->succ->pred=(NODE)->pred;}

#define GC_INSERT_NODE( NODE,SUCC ){\
(NODE)->pred=(SUCC)->pred;\
(NODE)->succ=(SUCC);\
(SUCC)->pred->succ=(NODE);\
(SUCC)->pred=(NODE);}

void gc_init1(){
	GC_CLEAR_LIST( gc_free_list );
	GC_CLEAR_LIST( gc_marked_list );
	GC_CLEAR_LIST( gc_unmarked_list);
	GC_CLEAR_LIST( gc_queued_list );
}

void gc_init2(){
	gc_mark_roots();
}

#if CFG_CPP_GC_MODE==2

struct gc_ctor{
	gc_ctor(){ ++gc_ctor_nest; }
	~gc_ctor(){ --gc_ctor_nest; }
};

struct gc_enter{
	gc_object **sp;
	gc_enter():sp(gc_locals_sp){
	}
	~gc_enter(){
	/*
		static int max_locals;
		int n=gc_locals_sp-gc_locals;
		if( n>max_locals ){
			max_locals=n;
			printf( "max_locals=%i\n",n );
		}
	*/
		gc_locals_sp=sp;
	}
};

#define GC_CTOR gc_ctor _c;
#define GC_ENTER gc_enter _e;

#else

struct gc_ctor{
};
struct gc_enter{
};

#define GC_CTOR
#define GC_ENTER

#endif

void gc_flush_free( int size ){

	int t=gc_free_bytes-size;
	if( t<0 ) t=0;
	
	while( gc_free_bytes>t ){
		gc_object *p=gc_free_list.succ;
		if( !p || p==&gc_free_list ){
//			printf("ERROR:p=%p gc_free_bytes=%i\n",p,gc_free_bytes);
//			fflush(stdout);
			gc_free_bytes=0;
			break;
		}
		GC_REMOVE_NODE(p);
		delete p;	//...to gc_free
	}
}

void *gc_ext_malloc( int size ){

	gc_new_bytes+=size;
	
	gc_flush_free( size );
	
	return malloc( size );
}

void gc_ext_malloced( int size ){

	gc_new_bytes+=size;
	
	gc_flush_free( size );
}

gc_object *gc_object_alloc( int size ){

	size=(size+7)&~7;
	
#if CFG_CPP_GC_MODE==1

	gc_new_bytes+=size;
	
#elif CFG_CPP_GC_MODE==2

	if( !gc_ctor_nest ){
#if DEBUG_GC
		int ms=gc_micros();
#endif
		if( gc_new_bytes+size>(CFG_CPP_GC_TRIGGER) ){
			gc_collect_all();
			gc_new_bytes=size;
		}else{
			gc_new_bytes+=size;
			gc_mark_queued( (long long)(gc_new_bytes)*(gc_alloced_bytes-gc_new_bytes)/(CFG_CPP_GC_TRIGGER)+gc_new_bytes );
		}
		
#if DEBUG_GC
		ms=gc_micros()-ms;
		if( ms>=100 ) {printf( "gc time:%i\n",ms );fflush( stdout );}
#endif
	}

#endif

	gc_flush_free( size );

	gc_object *p;
	if( size<64 && (p=gc_cache[size>>3]) ){
		gc_cache[size>>3]=p->succ;
	}else{
		p=(gc_object*)malloc( size );
	}
	
	p->flags=size|gc_markbit;
	GC_INSERT_NODE( p,&gc_unmarked_list );

	gc_alloced_bytes+=size;
	if( gc_alloced_bytes>gc_max_alloced_bytes ) gc_max_alloced_bytes=gc_alloced_bytes;
	
#if CFG_CPP_GC_MODE==2
	*gc_locals_sp++=p;
#endif

	return p;
}

void gc_object_free( gc_object *p ){

	int size=p->flags & ~7;
	gc_free_bytes-=size;
	
	if( size<64 ){
		p->succ=gc_cache[size>>3];
		gc_cache[size>>3]=p;
	}else{
		free( p );
	}
}

template<class T> void gc_mark( T *t ){

	gc_object *p=dynamic_cast<gc_object*>(t);
	
	if( p && (p->flags & 3)==gc_markbit ){
		p->flags^=1;
		GC_REMOVE_NODE( p );
		GC_INSERT_NODE( p,&gc_marked_list );
		gc_marked_bytes+=(p->flags & ~7);
		p->mark();
	}
}

template<class T> void gc_mark_q( T *t ){

	gc_object *p=dynamic_cast<gc_object*>(t);
	
	if( p && (p->flags & 3)==gc_markbit ){
		p->flags^=1;
		GC_REMOVE_NODE( p );
		GC_INSERT_NODE( p,&gc_queued_list );
	}
}

template<class T> T *gc_retain( T *t ){
#if CFG_CPP_GC_MODE==2
	*gc_locals_sp++=dynamic_cast<gc_object*>( t );
#endif	
	return t;
}

template<class T,class V> void gc_assign( T *&lhs,V *rhs ){
	gc_object *p=dynamic_cast<gc_object*>(rhs);
	if( p && (p->flags & 3)==gc_markbit ){
		p->flags^=1;
		GC_REMOVE_NODE( p );
		GC_INSERT_NODE( p,&gc_queued_list );
	}
	lhs=rhs;
}

void gc_mark_locals(){
	for( gc_object **pp=gc_locals;pp!=gc_locals_sp;++pp ){
		gc_object *p=*pp;
		if( p && (p->flags & 3)==gc_markbit ){
			p->flags^=1;
			GC_REMOVE_NODE( p );
			GC_INSERT_NODE( p,&gc_marked_list );
			gc_marked_bytes+=(p->flags & ~7);
			p->mark();
		}
	}
}

void gc_mark_queued( int n ){
	while( gc_marked_bytes<n && !GC_LIST_IS_EMPTY( gc_queued_list ) ){
		gc_object *p=gc_queued_list.succ;
		GC_REMOVE_NODE( p );
		GC_INSERT_NODE( p,&gc_marked_list );
		gc_marked_bytes+=(p->flags & ~7);
		p->mark();
	}
}

//returns reclaimed bytes
int gc_sweep(){

	int reclaimed_bytes=gc_alloced_bytes-gc_marked_bytes;
	
	if( reclaimed_bytes ){
	
		//append unmarked list to end of free list
		gc_object *head=gc_unmarked_list.succ;
		gc_object *tail=gc_unmarked_list.pred;
		gc_object *succ=&gc_free_list;
		gc_object *pred=succ->pred;
		head->pred=pred;
		tail->succ=succ;
		pred->succ=head;
		succ->pred=tail;
		
		gc_free_bytes+=reclaimed_bytes;
	}
	
	//move marked to unmarked.
	gc_unmarked_list=gc_marked_list;
	gc_unmarked_list.succ->pred=gc_unmarked_list.pred->succ=&gc_unmarked_list;
	
	//clear marked.
	GC_CLEAR_LIST( gc_marked_list );
	
	//adjust sizes
	gc_alloced_bytes=gc_marked_bytes;
	gc_marked_bytes=0;
	gc_markbit^=1;
	
	return reclaimed_bytes;
}

void gc_collect_all(){

//	printf( "Mark locals\n" );fflush( stdout );
	gc_mark_locals();

//	printf( "Mark queued\n" );fflush( stdout );
	gc_mark_queued( 0x7fffffff );

//	printf( "sweep\n" );fflush( stdout );	
	gc_sweep();

//	printf( "Mark roots\n" );fflush( stdout );
	gc_mark_roots();

#if DEBUG_GC	
	printf( "gc collected:%i\n",reclaimed );fflush( stdout );
#endif
}

void gc_collect(){

	if( gc_locals_sp!=gc_locals ){
//		printf( "GC_LOCALS error\n" );fflush( stdout );
		gc_locals_sp=gc_locals;
	}
	
#if CFG_CPP_GC_MODE==1

#if DEBUG_GC
	int ms=gc_micros();
#endif

	if( gc_new_bytes>(CFG_CPP_GC_TRIGGER) ){
		gc_collect_all();
		gc_new_bytes=0;
	}else{
		gc_mark_queued( (long long)(gc_new_bytes)*(gc_alloced_bytes-gc_new_bytes)/(CFG_CPP_GC_TRIGGER)+gc_new_bytes );
	}

#if DEBUG_GC
	ms=gc_micros()-ms;
	if( ms>=100 ) {printf( "gc time:%i\n",ms );fflush( stdout );}
#endif

#endif

}

// ***** Array *****

template<class T> T *t_memcpy( T *dst,const T *src,int n ){
	memcpy( dst,src,n*sizeof(T) );
	return dst+n;
}

template<class T> T *t_memset( T *dst,int val,int n ){
	memset( dst,val,n*sizeof(T) );
	return dst+n;
}

template<class T> int t_memcmp( const T *x,const T *y,int n ){
	return memcmp( x,y,n*sizeof(T) );
}

template<class T> int t_strlen( const T *p ){
	const T *q=p++;
	while( *q++ ){}
	return q-p;
}

template<class T> T *t_create( int n,T *p ){
	t_memset( p,0,n );
	return p+n;
}

template<class T> T *t_create( int n,T *p,const T *q ){
	t_memcpy( p,q,n );
	return p+n;
}

template<class T> void t_destroy( int n,T *p ){
}

template<class T> void gc_mark_elements( int n,T *p ){
}

template<class T> void gc_mark_elements( int n,T **p ){
	for( int i=0;i<n;++i ) gc_mark( p[i] );
}

template<class T> class Array{
public:
	Array():rep( &nullRep ){
	}

	//Uses default...
//	Array( const Array<T> &t )...
	
	Array( int length ):rep( Rep::alloc( length ) ){
		t_create( rep->length,rep->data );
	}
	
	Array( const T *p,int length ):rep( Rep::alloc(length) ){
		t_create( rep->length,rep->data,p );
	}
	
	~Array(){
	}

	//Uses default...
//	Array &operator=( const Array &t )...
	
	int Length()const{ 
		return rep->length; 
	}
	
	T &At( int index ){
		if( index<0 || index>=rep->length ) dbg_error( "Array index out of range" );
		return rep->data[index]; 
	}
	
	const T &At( int index )const{
		if( index<0 || index>=rep->length ) dbg_error( "Array index out of range" );
		return rep->data[index]; 
	}
	
	T &operator[]( int index ){
		return rep->data[index]; 
	}

	const T &operator[]( int index )const{
		return rep->data[index]; 
	}
	
	Array Slice( int from,int term )const{
		int len=rep->length;
		if( from<0 ){ 
			from+=len;
			if( from<0 ) from=0;
		}else if( from>len ){
			from=len;
		}
		if( term<0 ){
			term+=len;
		}else if( term>len ){
			term=len;
		}
		if( term<=from ) return Array();
		return Array( rep->data+from,term-from );
	}

	Array Slice( int from )const{
		return Slice( from,rep->length );
	}
	
	Array Resize( int newlen )const{
		if( newlen<=0 ) return Array();
		int n=rep->length;
		if( newlen<n ) n=newlen;
		Rep *p=Rep::alloc( newlen );
		T *q=p->data;
		q=t_create( n,q,rep->data );
		q=t_create( (newlen-n),q );
		return Array( p );
	}
	
private:
	struct Rep : public gc_object{
		int length;
		T data[0];
		
		Rep():length(0){
			flags=3;
		}
		
		Rep( int length ):length(length){
		}
		
		~Rep(){
			t_destroy( length,data );
		}
		
		void mark(){
			gc_mark_elements( length,data );
		}
		
		static Rep *alloc( int length ){
			if( !length ) return &nullRep;
			void *p=gc_object_alloc( sizeof(Rep)+length*sizeof(T) );
			return ::new(p) Rep( length );
		}
		
	};
	Rep *rep;
	
	static Rep nullRep;
	
	template<class C> friend void gc_mark( Array<C> t );
	template<class C> friend void gc_mark_q( Array<C> t );
	template<class C> friend Array<C> gc_retain( Array<C> t );
	template<class C> friend void gc_assign( Array<C> &lhs,Array<C> rhs );
	template<class C> friend void gc_mark_elements( int n,Array<C> *p );
	
	Array( Rep *rep ):rep(rep){
	}
};

template<class T> typename Array<T>::Rep Array<T>::nullRep;

template<class T> Array<T> *t_create( int n,Array<T> *p ){
	for( int i=0;i<n;++i ) *p++=Array<T>();
	return p;
}

template<class T> Array<T> *t_create( int n,Array<T> *p,const Array<T> *q ){
	for( int i=0;i<n;++i ) *p++=*q++;
	return p;
}

template<class T> void gc_mark( Array<T> t ){
	gc_mark( t.rep );
}

template<class T> void gc_mark_q( Array<T> t ){
	gc_mark_q( t.rep );
}

template<class T> Array<T> gc_retain( Array<T> t ){
#if CFG_CPP_GC_MODE==2
	gc_retain( t.rep );
#endif
	return t;
}

template<class T> void gc_assign( Array<T> &lhs,Array<T> rhs ){
	gc_mark( rhs.rep );
	lhs=rhs;
}

template<class T> void gc_mark_elements( int n,Array<T> *p ){
	for( int i=0;i<n;++i ) gc_mark( p[i].rep );
}
		
// ***** String *****

static const char *_str_load_err;

class String{
public:
	String():rep( &nullRep ){
	}
	
	String( const String &t ):rep( t.rep ){
		rep->retain();
	}

	String( int n ){
		char buf[256];
		sprintf_s( buf,"%i",n );
		rep=Rep::alloc( t_strlen(buf) );
		for( int i=0;i<rep->length;++i ) rep->data[i]=buf[i];
	}
	
	String( Float n ){
		char buf[256];
		
		//would rather use snprintf, but it's doing weird things in MingW.
		//
		sprintf_s( buf,"%.17lg",n );
		//
		char *p;
		for( p=buf;*p;++p ){
			if( *p=='.' || *p=='e' ) break;
		}
		if( !*p ){
			*p++='.';
			*p++='0';
			*p=0;
		}

		rep=Rep::alloc( t_strlen(buf) );
		for( int i=0;i<rep->length;++i ) rep->data[i]=buf[i];
	}

	String( Char ch,int length ):rep( Rep::alloc(length) ){
		for( int i=0;i<length;++i ) rep->data[i]=ch;
	}

	String( const Char *p ):rep( Rep::alloc(t_strlen(p)) ){
		t_memcpy( rep->data,p,rep->length );
	}

	String( const Char *p,int length ):rep( Rep::alloc(length) ){
		t_memcpy( rep->data,p,rep->length );
	}
	
#if __OBJC__	
	String( NSString *nsstr ):rep( Rep::alloc([nsstr length]) ){
		unichar *buf=(unichar*)malloc( rep->length * sizeof(unichar) );
		[nsstr getCharacters:buf range:NSMakeRange(0,rep->length)];
		for( int i=0;i<rep->length;++i ) rep->data[i]=buf[i];
		free( buf );
	}
#endif

#if __cplusplus_winrt
	String( Platform::String ^str ):rep( Rep::alloc(str->Length()) ){
		for( int i=0;i<rep->length;++i ) rep->data[i]=str->Data()[i];
	}
#endif

	~String(){
		rep->release();
	}
	
	template<class C> String( const C *p ):rep( Rep::alloc(t_strlen(p)) ){
		for( int i=0;i<rep->length;++i ) rep->data[i]=p[i];
	}
	
	template<class C> String( const C *p,int length ):rep( Rep::alloc(length) ){
		for( int i=0;i<rep->length;++i ) rep->data[i]=p[i];
	}
	
	int Length()const{
		return rep->length;
	}
	
	const Char *Data()const{
		return rep->data;
	}
	
	Char At( int index )const{
		if( index<0 || index>=rep->length ) dbg_error( "Character index out of range" );
		return rep->data[index]; 
	}
	
	Char operator[]( int index )const{
		return rep->data[index];
	}
	
	String &operator=( const String &t ){
		t.rep->retain();
		rep->release();
		rep=t.rep;
		return *this;
	}
	
	String &operator+=( const String &t ){
		return operator=( *this+t );
	}
	
	int Compare( const String &t )const{
		int n=rep->length<t.rep->length ? rep->length : t.rep->length;
		for( int i=0;i<n;++i ){
			if( int q=(int)(rep->data[i])-(int)(t.rep->data[i]) ) return q;
		}
		return rep->length-t.rep->length;
	}
	
	bool operator==( const String &t )const{
		return rep->length==t.rep->length && t_memcmp( rep->data,t.rep->data,rep->length )==0;
	}
	
	bool operator!=( const String &t )const{
		return rep->length!=t.rep->length || t_memcmp( rep->data,t.rep->data,rep->length )!=0;
	}
	
	bool operator<( const String &t )const{
		return Compare( t )<0;
	}
	
	bool operator<=( const String &t )const{
		return Compare( t )<=0;
	}
	
	bool operator>( const String &t )const{
		return Compare( t )>0;
	}
	
	bool operator>=( const String &t )const{
		return Compare( t )>=0;
	}
	
	String operator+( const String &t )const{
		if( !rep->length ) return t;
		if( !t.rep->length ) return *this;
		Rep *p=Rep::alloc( rep->length+t.rep->length );
		Char *q=p->data;
		q=t_memcpy( q,rep->data,rep->length );
		q=t_memcpy( q,t.rep->data,t.rep->length );
		return String( p );
	}
	
	int Find( String find,int start=0 )const{
		if( start<0 ) start=0;
		while( start+find.rep->length<=rep->length ){
			if( !t_memcmp( rep->data+start,find.rep->data,find.rep->length ) ) return start;
			++start;
		}
		return -1;
	}
	
	int FindLast( String find )const{
		int start=rep->length-find.rep->length;
		while( start>=0 ){
			if( !t_memcmp( rep->data+start,find.rep->data,find.rep->length ) ) return start;
			--start;
		}
		return -1;
	}
	
	int FindLast( String find,int start )const{
		if( start>rep->length-find.rep->length ) start=rep->length-find.rep->length;
		while( start>=0 ){
			if( !t_memcmp( rep->data+start,find.rep->data,find.rep->length ) ) return start;
			--start;
		}
		return -1;
	}
	
	String Trim()const{
		int i=0,i2=rep->length;
		while( i<i2 && rep->data[i]<=32 ) ++i;
		while( i2>i && rep->data[i2-1]<=32 ) --i2;
		if( i==0 && i2==rep->length ) return *this;
		return String( rep->data+i,i2-i );
	}

	Array<String> Split( String sep )const{
	
		if( !sep.rep->length ){
			Array<String> bits( rep->length );
			for( int i=0;i<rep->length;++i ){
				bits[i]=String( (Char)(*this)[i],1 );
			}
			return bits;
		}
		
		int i=0,i2,n=1;
		while( (i2=Find( sep,i ))!=-1 ){
			++n;
			i=i2+sep.rep->length;
		}
		Array<String> bits( n );
		if( n==1 ){
			bits[0]=*this;
			return bits;
		}
		i=0;n=0;
		while( (i2=Find( sep,i ))!=-1 ){
			bits[n++]=Slice( i,i2 );
			i=i2+sep.rep->length;
		}
		bits[n]=Slice( i );
		return bits;
	}

	String Join( Array<String> bits )const{
		if( bits.Length()==0 ) return String();
		if( bits.Length()==1 ) return bits[0];
		int newlen=rep->length * (bits.Length()-1);
		for( int i=0;i<bits.Length();++i ){
			newlen+=bits[i].rep->length;
		}
		Rep *p=Rep::alloc( newlen );
		Char *q=p->data;
		q=t_memcpy( q,bits[0].rep->data,bits[0].rep->length );
		for( int i=1;i<bits.Length();++i ){
			q=t_memcpy( q,rep->data,rep->length );
			q=t_memcpy( q,bits[i].rep->data,bits[i].rep->length );
		}
		return String( p );
	}

	String Replace( String find,String repl )const{
		int i=0,i2,newlen=0;
		while( (i2=Find( find,i ))!=-1 ){
			newlen+=(i2-i)+repl.rep->length;
			i=i2+find.rep->length;
		}
		if( !i ) return *this;
		newlen+=rep->length-i;
		Rep *p=Rep::alloc( newlen );
		Char *q=p->data;
		i=0;
		while( (i2=Find( find,i ))!=-1 ){
			q=t_memcpy( q,rep->data+i,i2-i );
			q=t_memcpy( q,repl.rep->data,repl.rep->length );
			i=i2+find.rep->length;
		}
		q=t_memcpy( q,rep->data+i,rep->length-i );
		return String( p );
	}

	String ToLower()const{
		for( int i=0;i<rep->length;++i ){
			Char t=towlower( rep->data[i] );
			if( t==rep->data[i] ) continue;
			Rep *p=Rep::alloc( rep->length );
			Char *q=p->data;
			t_memcpy( q,rep->data,i );
			for( q[i++]=t;i<rep->length;++i ){
				q[i]=towlower( rep->data[i] );
			}
			return String( p );
		}
		return *this;
	}

	String ToUpper()const{
		for( int i=0;i<rep->length;++i ){
			Char t=towupper( rep->data[i] );
			if( t==rep->data[i] ) continue;
			Rep *p=Rep::alloc( rep->length );
			Char *q=p->data;
			t_memcpy( q,rep->data,i );
			for( q[i++]=t;i<rep->length;++i ){
				q[i]=towupper( rep->data[i] );
			}
			return String( p );
		}
		return *this;
	}
	
	bool Contains( String sub )const{
		return Find( sub )!=-1;
	}

	bool StartsWith( String sub )const{
		return sub.rep->length<=rep->length && !t_memcmp( rep->data,sub.rep->data,sub.rep->length );
	}

	bool EndsWith( String sub )const{
		return sub.rep->length<=rep->length && !t_memcmp( rep->data+rep->length-sub.rep->length,sub.rep->data,sub.rep->length );
	}
	
	String Slice( int from,int term )const{
		int len=rep->length;
		if( from<0 ){
			from+=len;
			if( from<0 ) from=0;
		}else if( from>len ){
			from=len;
		}
		if( term<0 ){
			term+=len;
		}else if( term>len ){
			term=len;
		}
		if( term<from ) return String();
		if( from==0 && term==len ) return *this;
		return String( rep->data+from,term-from );
	}

	String Slice( int from )const{
		return Slice( from,rep->length );
	}
	
	Array<int> ToChars()const{
		Array<int> chars( rep->length );
		for( int i=0;i<rep->length;++i ) chars[i]=rep->data[i];
		return chars;
	}
	
	int ToInt()const{
		char buf[64];
		return atoi( ToCString<char>( buf,sizeof(buf) ) );
	}
	
	Float ToFloat()const{
		char buf[256];
		return atof( ToCString<char>( buf,sizeof(buf) ) );
	}

	template<class C> class CString{
		struct Rep{
			int refs;
			C data[1];
		};
		Rep *_rep;
		static Rep _nul;
	public:
		template<class T> CString( const T *data,int length ){
			_rep=(Rep*)malloc( length*sizeof(C)+sizeof(Rep) );
			_rep->refs=1;
			_rep->data[length]=0;
			for( int i=0;i<length;++i ){
				_rep->data[i]=(C)data[i];
			}
		}
		CString():_rep( new Rep ){
			_rep->refs=1;
		}
		CString( const CString &c ):_rep(c._rep){
			++_rep->refs;
		}
		~CString(){
			if( !--_rep->refs ) free( _rep );
		}
		CString &operator=( const CString &c ){
			++c._rep->refs;
			if( !--_rep->refs ) free( _rep );
			_rep=c._rep;
			return *this;
		}
		operator const C*()const{ 
			return _rep->data;
		}
	};
	
	template<class C> CString<C> ToCString()const{
		return CString<C>( rep->data,rep->length );
	}

	template<class C> C *ToCString( C *p,int length )const{
		if( --length>rep->length ) length=rep->length;
		for( int i=0;i<length;++i ) p[i]=rep->data[i];
		p[length]=0;
		return p;
	}
	
#if __OBJC__	
	NSString *ToNSString()const{
		return [NSString stringWithCharacters:ToCString<unichar>() length:rep->length];
	}
#endif

#if __cplusplus_winrt
	Platform::String ^ToWinRTString()const{
		return ref new Platform::String( rep->data,rep->length );
	}
#endif

	bool Save( FILE *fp ){
		std::vector<unsigned char> buf;
		Save( buf );
		return buf.size() ? fwrite( &buf[0],1,buf.size(),fp )==buf.size() : true;
	}
	
	void Save( std::vector<unsigned char> &buf ){
	
		Char *p=rep->data;
		Char *e=p+rep->length;
		
		while( p<e ){
			Char c=*p++;
			if( c<0x80 ){
				buf.push_back( c );
			}else if( c<0x800 ){
				buf.push_back( 0xc0 | (c>>6) );
				buf.push_back( 0x80 | (c & 0x3f) );
			}else{
				buf.push_back( 0xe0 | (c>>12) );
				buf.push_back( 0x80 | ((c>>6) & 0x3f) );
				buf.push_back( 0x80 | (c & 0x3f) );
			}
		}
	}
	
	static String FromChars( Array<int> chars ){
		int n=chars.Length();
		Rep *p=Rep::alloc( n );
		for( int i=0;i<n;++i ){
			p->data[i]=chars[i];
		}
		return String( p );
	}

	static String Load( FILE *fp ){
		unsigned char tmp[4096];
		std::vector<unsigned char> buf;
		for(;;){
			int n=fread( tmp,1,4096,fp );
			if( n>0 ) buf.insert( buf.end(),tmp,tmp+n );
			if( n!=4096 ) break;
		}
		return buf.size() ? String::Load( &buf[0],buf.size() ) : String();
	}
	
	static String Load( unsigned char *p,int n ){
	
		_str_load_err=0;
		
		unsigned char *e=p+n;
		std::vector<Char> chars;
		
		int t0=n>0 ? p[0] : -1;
		int t1=n>1 ? p[1] : -1;

		if( t0==0xfe && t1==0xff ){
			p+=2;
			while( p<e-1 ){
				int c=*p++;
				chars.push_back( (c<<8)|*p++ );
			}
		}else if( t0==0xff && t1==0xfe ){
			p+=2;
			while( p<e-1 ){
				int c=*p++;
				chars.push_back( (*p++<<8)|c );
			}
		}else{
			int t2=n>2 ? p[2] : -1;
			if( t0==0xef && t1==0xbb && t2==0xbf ) p+=3;
			unsigned char *q=p;
			bool fail=false;
			while( p<e ){
				unsigned int c=*p++;
				if( c & 0x80 ){
					if( (c & 0xe0)==0xc0 ){
						if( p>=e || (p[0] & 0xc0)!=0x80 ){
							fail=true;
							break;
						}
						c=((c & 0x1f)<<6) | (p[0] & 0x3f);
						p+=1;
					}else if( (c & 0xf0)==0xe0 ){
						if( p+1>=e || (p[0] & 0xc0)!=0x80 || (p[1] & 0xc0)!=0x80 ){
							fail=true;
							break;
						}
						c=((c & 0x0f)<<12) | ((p[0] & 0x3f)<<6) | (p[1] & 0x3f);
						p+=2;
					}else{
						fail=true;
						break;
					}
				}
				chars.push_back( c );
			}
			if( fail ){
				_str_load_err="Invalid UTF-8";
				return String( q,n );
			}
		}
		return chars.size() ? String( &chars[0],chars.size() ) : String();
	}

private:
	
	struct Rep{
		int refs;
		int length;
		Char data[0];
		
		Rep():refs(1),length(0){
		}
		
		Rep( int length ):refs(1),length(length){
		}
		
		void retain(){
			++refs;
		}
		
		void release(){
			if( --refs || !length ) return;
			free( this );
		}

		static Rep *alloc( int length ){
			if( !length ) return &nullRep;
			void *p=malloc( sizeof(Rep)+length*sizeof(Char) );
			return new(p) Rep( length );
		}
	};
	Rep *rep;
	
	static Rep nullRep;
	
	String( Rep *rep ):rep(rep){
	}
};

String::Rep String::nullRep;

String *t_create( int n,String *p ){
	for( int i=0;i<n;++i ) new( &p[i] ) String();
	return p+n;
}

String *t_create( int n,String *p,const String *q ){
	for( int i=0;i<n;++i ) new( &p[i] ) String( q[i] );
	return p+n;
}

void t_destroy( int n,String *p ){
	for( int i=0;i<n;++i ) p[i].~String();
}

// ***** Object *****

String dbg_stacktrace();

class Object : public gc_object{
public:
	virtual bool Equals( Object *obj ){
		return this==obj;
	}
	
	virtual int Compare( Object *obj ){
		return (char*)this-(char*)obj;
	}
	
	virtual String debug(){
		return "+Object\n";
	}
};

class ThrowableObject : public Object{
#ifndef NDEBUG
public:
	String stackTrace;
	ThrowableObject():stackTrace( dbg_stacktrace() ){}
#endif
};

struct gc_interface{
	virtual ~gc_interface(){}
};

//***** Debugger *****

//#define Error bbError
//#define Print bbPrint

int bbPrint( String t );

#define dbg_stream stderr

#if _MSC_VER
#define dbg_typeof decltype
#else
#define dbg_typeof __typeof__
#endif 

struct dbg_func;
struct dbg_var_type;

static int dbg_suspend;
static int dbg_stepmode;

const char *dbg_info;
String dbg_exstack;

static void *dbg_var_buf[65536*3];
static void **dbg_var_ptr=dbg_var_buf;

static dbg_func *dbg_func_buf[1024];
static dbg_func **dbg_func_ptr=dbg_func_buf;

String dbg_type( bool *p ){
	return "Bool";
}

String dbg_type( int *p ){
	return "Int";
}

String dbg_type( Float *p ){
	return "Float";
}

String dbg_type( String *p ){
	return "String";
}

template<class T> String dbg_type( T *p ){
	return "Object";
}

template<class T> String dbg_type( Array<T> *p ){
	return dbg_type( &(*p)[0] )+"[]";
}

String dbg_value( bool *p ){
	return *p ? "True" : "False";
}

String dbg_value( int *p ){
	return String( *p );
}

String dbg_value( Float *p ){
	return String( *p );
}

String dbg_value( String *p ){
	String t=*p;
	if( t.Length()>100 ) t=t.Slice( 0,100 )+"...";
	t=t.Replace( "\"","~q" );
	t=t.Replace( "\t","~t" );
	t=t.Replace( "\n","~n" );
	t=t.Replace( "\r","~r" );
	return String("\"")+t+"\"";
}

template<class T> String dbg_value( T *t ){
	Object *p=dynamic_cast<Object*>( *t );
	char buf[64];
	sprintf_s( buf,"%p",p );
	return String("@") + (buf[0]=='0' && buf[1]=='x' ? buf+2 : buf );
}

template<class T> String dbg_value( Array<T> *p ){
	String t="[";
	int n=(*p).Length();
	for( int i=0;i<n;++i ){
		if( i ) t+=",";
		t+=dbg_value( &(*p)[i] );
	}
	return t+"]";
}

template<class T> String dbg_decl( const char *id,T *ptr ){
	return String( id )+":"+dbg_type(ptr)+"="+dbg_value(ptr)+"\n";
}

struct dbg_var_type{
	virtual String type( void *p )=0;
	virtual String value( void *p )=0;
};

template<class T> struct dbg_var_type_t : public dbg_var_type{

	String type( void *p ){
		return dbg_type( (T*)p );
	}
	
	String value( void *p ){
		return dbg_value( (T*)p );
	}
	
	static dbg_var_type_t<T> info;
};
template<class T> dbg_var_type_t<T> dbg_var_type_t<T>::info;

struct dbg_blk{
	void **var_ptr;
	
	dbg_blk():var_ptr(dbg_var_ptr){
		if( dbg_stepmode=='l' ) --dbg_suspend;
	}
	
	~dbg_blk(){
		if( dbg_stepmode=='l' ) ++dbg_suspend;
		dbg_var_ptr=var_ptr;
	}
};

struct dbg_func : public dbg_blk{
	const char *id;
	const char *info;

	dbg_func( const char *p ):id(p),info(dbg_info){
		*dbg_func_ptr++=this;
		if( dbg_stepmode=='s' ) --dbg_suspend;
	}
	
	~dbg_func(){
		if( dbg_stepmode=='s' ) ++dbg_suspend;
		--dbg_func_ptr;
		dbg_info=info;
	}
};

int dbg_print( String t ){
	static char *buf;
	static int len;
	int n=t.Length();
	if( n+100>len ){
		len=n+100;
		free( buf );
		buf=(char*)malloc( len );
	}
	buf[n]='\n';
	for( int i=0;i<n;++i ) buf[i]=t[i];
	fwrite( buf,n+1,1,dbg_stream );
	fflush( dbg_stream );
	return 0;
}

void dbg_callstack(){

	void **var_ptr=dbg_var_buf;
	dbg_func **func_ptr=dbg_func_buf;
	
	while( var_ptr!=dbg_var_ptr ){
		while( func_ptr!=dbg_func_ptr && var_ptr==(*func_ptr)->var_ptr ){
			const char *id=(*func_ptr++)->id;
			const char *info=func_ptr!=dbg_func_ptr ? (*func_ptr)->info : dbg_info;
			fprintf( dbg_stream,"+%s;%s\n",id,info );
		}
		void *vp=*var_ptr++;
		const char *nm=(const char*)*var_ptr++;
		dbg_var_type *ty=(dbg_var_type*)*var_ptr++;
		dbg_print( String(nm)+":"+ty->type(vp)+"="+ty->value(vp) );
	}
	while( func_ptr!=dbg_func_ptr ){
		const char *id=(*func_ptr++)->id;
		const char *info=func_ptr!=dbg_func_ptr ? (*func_ptr)->info : dbg_info;
		fprintf( dbg_stream,"+%s;%s\n",id,info );
	}
}

String dbg_stacktrace(){
	if( !dbg_info || !dbg_info[0] ) return "";
	String str=String( dbg_info )+"\n";
	dbg_func **func_ptr=dbg_func_ptr;
	if( func_ptr==dbg_func_buf ) return str;
	while( --func_ptr!=dbg_func_buf ){
		str+=String( (*func_ptr)->info )+"\n";
	}
	return str;
}

void dbg_throw( const char *err ){
	dbg_exstack=dbg_stacktrace();
	throw err;
}

void dbg_stop(){

#if TARGET_OS_IPHONE
	dbg_throw( "STOP" );
#endif

	fprintf( dbg_stream,"{{~~%s~~}}\n",dbg_info );
	dbg_callstack();
	dbg_print( "" );
	
	for(;;){

		char buf[256];
		char *e=fgets( buf,256,stdin );
		if( !e ) exit( -1 );
		
		e=strchr( buf,'\n' );
		if( !e ) exit( -1 );
		
		*e=0;
		
		Object *p;
		
		switch( buf[0] ){
		case '?':
			break;
		case 'r':	//run
			dbg_suspend=0;		
			dbg_stepmode=0;
			return;
		case 's':	//step
			dbg_suspend=1;
			dbg_stepmode='s';
			return;
		case 'e':	//enter func
			dbg_suspend=1;
			dbg_stepmode='e';
			return;
		case 'l':	//leave block
			dbg_suspend=0;
			dbg_stepmode='l';
			return;
		case '@':	//dump object
			p=0;
			sscanf_s( buf+1,"%p",&p );
			if( p ){
				dbg_print( p->debug() );
			}else{
				dbg_print( "" );
			}
			break;
		case 'q':	//quit!
			exit( 0 );
			break;			
		default:
			printf( "????? %s ?????",buf );fflush( stdout );
			exit( -1 );
		}
	}
}

void dbg_error( const char *err ){

#if TARGET_OS_IPHONE
	dbg_throw( err );
#endif

	for(;;){
		bbPrint( String("Monkey Runtime Error : ")+err );
		bbPrint( dbg_stacktrace() );
		dbg_stop();
	}
}

#define DBG_INFO(X) dbg_info=(X);if( dbg_suspend>0 ) dbg_stop();

#define DBG_ENTER(P) dbg_func _dbg_func(P);

#define DBG_BLOCK() dbg_blk _dbg_blk;

#define DBG_GLOBAL( ID,NAME )	//TODO!

#define DBG_LOCAL( ID,NAME )\
*dbg_var_ptr++=&ID;\
*dbg_var_ptr++=(void*)NAME;\
*dbg_var_ptr++=&dbg_var_type_t<dbg_typeof(ID)>::info;

//**** main ****

int argc;
const char **argv;

Float D2R=0.017453292519943295f;
Float R2D=57.29577951308232f;

int bbPrint( String t ){

	static std::vector<unsigned char> buf;
	buf.clear();
	t.Save( buf );
	buf.push_back( '\n' );
	buf.push_back( 0 );
	
#if __cplusplus_winrt	//winrt?

#if CFG_WINRT_PRINT_ENABLED
	OutputDebugStringA( (const char*)&buf[0] );
#endif

#elif _WIN32			//windows?

	fputs( (const char*)&buf[0],stdout );
	fflush( stdout );

#elif __APPLE__			//macos/ios?

	fputs( (const char*)&buf[0],stdout );
	fflush( stdout );
	
#elif __linux			//linux?

#if CFG_ANDROID_NDK_PRINT_ENABLED
	LOGI( (const char*)&buf[0] );
#else
	fputs( (const char*)&buf[0],stdout );
	fflush( stdout );
#endif

#endif

	return 0;
}

class BBExitApp{
};

int bbError( String err ){
	if( !err.Length() ){
#if __cplusplus_winrt
		throw BBExitApp();
#else
		exit( 0 );
#endif
	}
	dbg_error( err.ToCString<char>() );
	return 0;
}

int bbDebugLog( String t ){
	bbPrint( t );
	return 0;
}

int bbDebugStop(){
	dbg_stop();
	return 0;
}

int bbInit();
int bbMain();

#if _MSC_VER

static void _cdecl seTranslator( unsigned int ex,EXCEPTION_POINTERS *p ){

	switch( ex ){
	case EXCEPTION_ACCESS_VIOLATION:dbg_error( "Memory access violation" );
	case EXCEPTION_ILLEGAL_INSTRUCTION:dbg_error( "Illegal instruction" );
	case EXCEPTION_INT_DIVIDE_BY_ZERO:dbg_error( "Integer divide by zero" );
	case EXCEPTION_STACK_OVERFLOW:dbg_error( "Stack overflow" );
	}
	dbg_error( "Unknown exception" );
}

#else

void sighandler( int sig  ){
	switch( sig ){
	case SIGSEGV:dbg_error( "Memory access violation" );
	case SIGILL:dbg_error( "Illegal instruction" );
	case SIGFPE:dbg_error( "Floating point exception" );
#if !_WIN32
	case SIGBUS:dbg_error( "Bus error" );
#endif	
	}
	dbg_error( "Unknown signal" );
}

#endif

//entry point call by target main()...
//
int bb_std_main( int argc,const char **argv ){

	::argc=argc;
	::argv=argv;
	
#if _MSC_VER

	_set_se_translator( seTranslator );

#else
	
	signal( SIGSEGV,sighandler );
	signal( SIGILL,sighandler );
	signal( SIGFPE,sighandler );
#if !_WIN32
	signal( SIGBUS,sighandler );
#endif

#endif

	if( !setlocale( LC_CTYPE,"en_US.UTF-8" ) ){
		setlocale( LC_CTYPE,"" );
	}

	gc_init1();

	bbInit();
	
	gc_init2();

	bbMain();

	return 0;
}


//***** game.h *****

struct BBGameEvent{
	enum{
		None=0,
		KeyDown=1,KeyUp=2,KeyChar=3,
		MouseDown=4,MouseUp=5,MouseMove=6,
		TouchDown=7,TouchUp=8,TouchMove=9,
		MotionAccel=10
	};
};

class BBGameDelegate : public Object{
public:
	virtual void StartGame(){}
	virtual void SuspendGame(){}
	virtual void ResumeGame(){}
	virtual void UpdateGame(){}
	virtual void RenderGame(){}
	virtual void KeyEvent( int event,int data ){}
	virtual void MouseEvent( int event,int data,float x,float y ){}
	virtual void TouchEvent( int event,int data,float x,float y ){}
	virtual void MotionEvent( int event,int data,float x,float y,float z ){}
	virtual void DiscardGraphics(){}
};

struct BBDisplayMode : public Object{
	int width;
	int height;
	int format;
	int hertz;
	int flags;
	BBDisplayMode( int width=0,int height=0,int format=0,int hertz=0,int flags=0 ):width(width),height(height),format(format),hertz(hertz),flags(flags){}
};

class BBGame{
public:
	BBGame();
	virtual ~BBGame(){}
	
	// ***** Extern *****
	static BBGame *Game(){ return _game; }
	
	virtual void SetDelegate( BBGameDelegate *delegate );
	virtual BBGameDelegate *Delegate(){ return _delegate; }
	
	virtual void SetKeyboardEnabled( bool enabled );
	virtual bool KeyboardEnabled();
	
	virtual void SetUpdateRate( int updateRate );
	virtual int UpdateRate();
	
	virtual bool Started(){ return _started; }
	virtual bool Suspended(){ return _suspended; }
	
	virtual int Millisecs();
	virtual void GetDate( Array<int> date );
	virtual int SaveState( String state );
	virtual String LoadState();
	virtual String LoadString( String path );
	virtual bool PollJoystick( int port,Array<Float> joyx,Array<Float> joyy,Array<Float> joyz,Array<bool> buttons );
	virtual void OpenUrl( String url );
	virtual void SetMouseVisible( bool visible );
	
	virtual int GetDeviceWidth(){ return 0; }
	virtual int GetDeviceHeight(){ return 0; }
	virtual void SetDeviceWindow( int width,int height,int flags ){}
	virtual Array<BBDisplayMode*> GetDisplayModes(){ return Array<BBDisplayMode*>(); }
	virtual BBDisplayMode *GetDesktopMode(){ return 0; }
	virtual void SetSwapInterval( int interval ){}

	// ***** Native *****	
	virtual String PathToFilePath( String path );
	virtual FILE *OpenFile( String path,String mode );
	virtual unsigned char *LoadData( String path,int *plength );
	virtual unsigned char *LoadImageData( String path,int *width,int *height,int *depth ){ return 0; }
	virtual unsigned char *LoadAudioData( String path,int *length,int *channels,int *format,int *hertz ){ return 0; }
	
	//***** Internal *****
	virtual void Die( ThrowableObject *ex );
	virtual void gc_collect();
	virtual void StartGame();
	virtual void SuspendGame();
	virtual void ResumeGame();
	virtual void UpdateGame();
	virtual void RenderGame();
	virtual void KeyEvent( int ev,int data );
	virtual void MouseEvent( int ev,int data,float x,float y );
	virtual void TouchEvent( int ev,int data,float x,float y );
	virtual void MotionEvent( int ev,int data,float x,float y,float z );
	virtual void DiscardGraphics();
	
protected:

	static BBGame *_game;

	BBGameDelegate *_delegate;
	bool _keyboardEnabled;
	int _updateRate;
	bool _started;
	bool _suspended;
};

//***** game.cpp *****

BBGame *BBGame::_game;

BBGame::BBGame():
_delegate( 0 ),
_keyboardEnabled( false ),
_updateRate( 0 ),
_started( false ),
_suspended( false ){
	_game=this;
}

void BBGame::SetDelegate( BBGameDelegate *delegate ){
	_delegate=delegate;
}

void BBGame::SetKeyboardEnabled( bool enabled ){
	_keyboardEnabled=enabled;
}

bool BBGame::KeyboardEnabled(){
	return _keyboardEnabled;
}

void BBGame::SetUpdateRate( int updateRate ){
	_updateRate=updateRate;
}

int BBGame::UpdateRate(){
	return _updateRate;
}

int BBGame::Millisecs(){
	return 0;
}

void BBGame::GetDate( Array<int> date ){
	int n=date.Length();
	if( n>0 ){
		time_t t=time( 0 );
		
#if _MSC_VER
		struct tm tii;
		struct tm *ti=&tii;
		localtime_s( ti,&t );
#else
		struct tm *ti=localtime( &t );
#endif

		date[0]=ti->tm_year+1900;
		if( n>1 ){ 
			date[1]=ti->tm_mon+1;
			if( n>2 ){
				date[2]=ti->tm_mday;
				if( n>3 ){
					date[3]=ti->tm_hour;
					if( n>4 ){
						date[4]=ti->tm_min;
						if( n>5 ){
							date[5]=ti->tm_sec;
							if( n>6 ){
								date[6]=0;
							}
						}
					}
				}
			}
		}
	}
}

int BBGame::SaveState( String state ){
	if( FILE *f=OpenFile( "./.monkeystate","wb" ) ){
		bool ok=state.Save( f );
		fclose( f );
		return ok ? 0 : -2;
	}
	return -1;
}

String BBGame::LoadState(){
	if( FILE *f=OpenFile( "./.monkeystate","rb" ) ){
		String str=String::Load( f );
		fclose( f );
		return str;
	}
	return "";
}

String BBGame::LoadString( String path ){
	if( FILE *fp=OpenFile( path,"rb" ) ){
		String str=String::Load( fp );
		fclose( fp );
		return str;
	}
	return "";
}

bool BBGame::PollJoystick( int port,Array<Float> joyx,Array<Float> joyy,Array<Float> joyz,Array<bool> buttons ){
	return false;
}

void BBGame::OpenUrl( String url ){
}

void BBGame::SetMouseVisible( bool visible ){
}

//***** C++ Game *****

String BBGame::PathToFilePath( String path ){
	return path;
}

FILE *BBGame::OpenFile( String path,String mode ){
	path=PathToFilePath( path );
	if( path=="" ) return 0;
	
#if __cplusplus_winrt
	path=path.Replace( "/","\\" );
	FILE *f;
	if( _wfopen_s( &f,path.ToCString<wchar_t>(),mode.ToCString<wchar_t>() ) ) return 0;
	return f;
#elif _WIN32
	return _wfopen( path.ToCString<wchar_t>(),mode.ToCString<wchar_t>() );
#else
	return fopen( path.ToCString<char>(),mode.ToCString<char>() );
#endif
}

unsigned char *BBGame::LoadData( String path,int *plength ){

	FILE *f=OpenFile( path,"rb" );
	if( !f ) return 0;

	const int BUF_SZ=4096;
	std::vector<void*> tmps;
	int length=0;
	
	for(;;){
		void *p=malloc( BUF_SZ );
		int n=fread( p,1,BUF_SZ,f );
		tmps.push_back( p );
		length+=n;
		if( n!=BUF_SZ ) break;
	}
	fclose( f );
	
	unsigned char *data=(unsigned char*)malloc( length );
	unsigned char *p=data;
	
	int sz=length;
	for( int i=0;i<tmps.size();++i ){
		int n=sz>BUF_SZ ? BUF_SZ : sz;
		memcpy( p,tmps[i],n );
		free( tmps[i] );
		sz-=n;
		p+=n;
	}
	
	*plength=length;
	
	gc_ext_malloced( length );
	
	return data;
}

//***** INTERNAL *****

void BBGame::Die( ThrowableObject *ex ){
	bbPrint( "Monkey Runtime Error : Uncaught Monkey Exception" );
#ifndef NDEBUG
	bbPrint( ex->stackTrace );
#endif
	exit( -1 );
}

void BBGame::gc_collect(){
	gc_mark( _delegate );
	::gc_collect();
}

void BBGame::StartGame(){

	if( _started ) return;
	_started=true;
	
	try{
		_delegate->StartGame();
	}catch( ThrowableObject *ex ){
		Die( ex );
	}
	gc_collect();
}

void BBGame::SuspendGame(){

	if( !_started || _suspended ) return;
	_suspended=true;
	
	try{
		_delegate->SuspendGame();
	}catch( ThrowableObject *ex ){
		Die( ex );
	}
	gc_collect();
}

void BBGame::ResumeGame(){

	if( !_started || !_suspended ) return;
	_suspended=false;
	
	try{
		_delegate->ResumeGame();
	}catch( ThrowableObject *ex ){
		Die( ex );
	}
	gc_collect();
}

void BBGame::UpdateGame(){

	if( !_started || _suspended ) return;
	
	try{
		_delegate->UpdateGame();
	}catch( ThrowableObject *ex ){
		Die( ex );
	}
	gc_collect();
}

void BBGame::RenderGame(){

	if( !_started ) return;
	
	try{
		_delegate->RenderGame();
	}catch( ThrowableObject *ex ){
		Die( ex );
	}
	gc_collect();
}

void BBGame::KeyEvent( int ev,int data ){

	if( !_started ) return;
	
	try{
		_delegate->KeyEvent( ev,data );
	}catch( ThrowableObject *ex ){
		Die( ex );
	}
	gc_collect();
}

void BBGame::MouseEvent( int ev,int data,float x,float y ){

	if( !_started ) return;
	
	try{
		_delegate->MouseEvent( ev,data,x,y );
	}catch( ThrowableObject *ex ){
		Die( ex );
	}
	gc_collect();
}

void BBGame::TouchEvent( int ev,int data,float x,float y ){

	if( !_started ) return;
	
	try{
		_delegate->TouchEvent( ev,data,x,y );
	}catch( ThrowableObject *ex ){
		Die( ex );
	}
	gc_collect();
}

void BBGame::MotionEvent( int ev,int data,float x,float y,float z ){

	if( !_started ) return;
	
	try{
		_delegate->MotionEvent( ev,data,x,y,z );
	}catch( ThrowableObject *ex ){
		Die( ex );
	}
	gc_collect();
}

void BBGame::DiscardGraphics(){

	if( !_started ) return;
	
	try{
		_delegate->DiscardGraphics();
	}catch( ThrowableObject *ex ){
		Die( ex );
	}
	gc_collect();
}


//***** wavloader.h *****
//
unsigned char *LoadWAV( FILE *f,int *length,int *channels,int *format,int *hertz );

//***** wavloader.cpp *****
//
static const char *readTag( FILE *f ){
	static char buf[8];
	if( fread( buf,4,1,f )!=1 ) return "";
	buf[4]=0;
	return buf;
}

static int readInt( FILE *f ){
	unsigned char buf[4];
	if( fread( buf,4,1,f )!=1 ) return -1;
	return (buf[3]<<24) | (buf[2]<<16) | (buf[1]<<8) | buf[0];
}

static int readShort( FILE *f ){
	unsigned char buf[2];
	if( fread( buf,2,1,f )!=1 ) return -1;
	return (buf[1]<<8) | buf[0];
}

static void skipBytes( int n,FILE *f ){
	char *p=(char*)malloc( n );
	fread( p,n,1,f );
	free( p );
}

unsigned char *LoadWAV( FILE *f,int *plength,int *pchannels,int *pformat,int *phertz ){
	if( !strcmp( readTag( f ),"RIFF" ) ){
		int len=readInt( f )-8;len=len;
		if( !strcmp( readTag( f ),"WAVE" ) ){
			if( !strcmp( readTag( f ),"fmt " ) ){
				int len2=readInt( f );
				int comp=readShort( f );
				if( comp==1 ){
					int chans=readShort( f );
					int hertz=readInt( f );
					int bytespersec=readInt( f );bytespersec=bytespersec;
					int pad=readShort( f );pad=pad;
					int bits=readShort( f );
					int format=bits/8;
					if( len2>16 ) skipBytes( len2-16,f );
					for(;;){
						const char *p=readTag( f );
						if( feof( f ) ) break;
						int size=readInt( f );
						if( strcmp( p,"data" ) ){
							skipBytes( size,f );
							continue;
						}
						unsigned char *data=(unsigned char*)malloc( size );
						if( fread( data,size,1,f )==1 ){
							*plength=size/(chans*format);
							*pchannels=chans;
							*pformat=format;
							*phertz=hertz;
							return data;
						}
						free( data );
					}
				}
			}
		}
	}
	return 0;
}



//***** oggloader.h *****
unsigned char *LoadOGG( FILE *f,int *length,int *channels,int *format,int *hertz );

//***** oggloader.cpp *****
unsigned char *LoadOGG( FILE *f,int *length,int *channels,int *format,int *hertz ){

	int error;
	stb_vorbis *v=stb_vorbis_open_file( f,0,&error,0 );
	if( !v ) return 0;
	
	stb_vorbis_info info=stb_vorbis_get_info( v );
	
	int limit=info.channels*4096;
	int offset=0,total=limit;

	short *data=(short*)malloc( total*2 );
	
	for(;;){
		int n=stb_vorbis_get_frame_short_interleaved( v,info.channels,data+offset,total-offset );
		if( !n ) break;
	
		offset+=n*info.channels;
		
		if( offset+limit>total ){
			total*=2;
			data=(short*)realloc( data,total*2 );
		}
	}
	
	data=(short*)realloc( data,offset*2 );
	
	*length=offset/info.channels;
	*channels=info.channels;
	*format=2;
	*hertz=info.sample_rate;
	
	stb_vorbis_close(v);
	
	return (unsigned char*)data;
}



//***** glfwgame.h *****

struct BBGlfwVideoMode : public Object{
	int Width;
	int Height;
	int RedBits;
	int GreenBits;
	int BlueBits;
	BBGlfwVideoMode( int w,int h,int r,int g,int b ):Width(w),Height(h),RedBits(r),GreenBits(g),BlueBits(b){}
};

class BBGlfwGame : public BBGame{
public:
	BBGlfwGame();

	static BBGlfwGame *GlfwGame(){ return _glfwGame; }
	
	virtual void SetUpdateRate( int hertz );
	virtual int Millisecs();
	virtual bool PollJoystick( int port,Array<Float> joyx,Array<Float> joyy,Array<Float> joyz,Array<bool> buttons );
	virtual void OpenUrl( String url );
	virtual void SetMouseVisible( bool visible );
	
	virtual int GetDeviceWidth();
	virtual int GetDeviceHeight();
	virtual void SetDeviceWindow( int width,int height,int flags );
	virtual Array<BBDisplayMode*> GetDisplayModes();
	virtual BBDisplayMode *GetDesktopMode();
	virtual void SetSwapInterval( int interval );

	virtual String PathToFilePath( String path );
	virtual unsigned char *LoadImageData( String path,int *width,int *height,int *depth );
	virtual unsigned char *LoadAudioData( String path,int *length,int *channels,int *format,int *hertz );
	
	virtual void SetGlfwWindow( int width,int height,int red,int green,int blue,int alpha,int depth,int stencil,bool fullscreen );
	virtual BBGlfwVideoMode *GetGlfwDesktopMode();
	virtual Array<BBGlfwVideoMode*> GetGlfwVideoModes();
	
	virtual void Run();
	
private:
	static BBGlfwGame *_glfwGame;

	double _updatePeriod;
	double _nextUpdate;
	
	int _swapInterval;
	
	void UpdateEvents();
		
protected:
	static int TransKey( int key );
	static int KeyToChar( int key );
	
	static void GLFWCALL OnKey( int key,int action );
	static void GLFWCALL OnChar( int chr,int action );
	static void GLFWCALL OnMouseButton( int button,int action );
	static void GLFWCALL OnMousePos( int x,int y );
	static int  GLFWCALL OnWindowClose();
};

//***** glfwgame.cpp *****

#define _QUOTE(X) #X
#define _STRINGIZE( X ) _QUOTE(X)

enum{
	VKEY_BACKSPACE=8,VKEY_TAB,
	VKEY_ENTER=13,
	VKEY_SHIFT=16,
	VKEY_CONTROL=17,
	VKEY_ESC=27,
	VKEY_SPACE=32,
	VKEY_PAGEUP=33,VKEY_PAGEDOWN,VKEY_END,VKEY_HOME,
	VKEY_LEFT=37,VKEY_UP,VKEY_RIGHT,VKEY_DOWN,
	VKEY_INSERT=45,VKEY_DELETE,
	VKEY_0=48,VKEY_1,VKEY_2,VKEY_3,VKEY_4,VKEY_5,VKEY_6,VKEY_7,VKEY_8,VKEY_9,
	VKEY_A=65,VKEY_B,VKEY_C,VKEY_D,VKEY_E,VKEY_F,VKEY_G,VKEY_H,VKEY_I,VKEY_J,
	VKEY_K,VKEY_L,VKEY_M,VKEY_N,VKEY_O,VKEY_P,VKEY_Q,VKEY_R,VKEY_S,VKEY_T,
	VKEY_U,VKEY_V,VKEY_W,VKEY_X,VKEY_Y,VKEY_Z,
	
	VKEY_LSYS=91,VKEY_RSYS,
	
	VKEY_NUM0=96,VKEY_NUM1,VKEY_NUM2,VKEY_NUM3,VKEY_NUM4,
	VKEY_NUM5,VKEY_NUM6,VKEY_NUM7,VKEY_NUM8,VKEY_NUM9,
	VKEY_NUMMULTIPLY=106,VKEY_NUMADD,VKEY_NUMSLASH,
	VKEY_NUMSUBTRACT,VKEY_NUMDECIMAL,VKEY_NUMDIVIDE,

	VKEY_F1=112,VKEY_F2,VKEY_F3,VKEY_F4,VKEY_F5,VKEY_F6,
	VKEY_F7,VKEY_F8,VKEY_F9,VKEY_F10,VKEY_F11,VKEY_F12,

	VKEY_LSHIFT=160,VKEY_RSHIFT,
	VKEY_LCONTROL=162,VKEY_RCONTROL,
	VKEY_LALT=164,VKEY_RALT,

	VKEY_TILDE=192,VKEY_MINUS=189,VKEY_EQUALS=187,
	VKEY_OPENBRACKET=219,VKEY_BACKSLASH=220,VKEY_CLOSEBRACKET=221,
	VKEY_SEMICOLON=186,VKEY_QUOTES=222,
	VKEY_COMMA=188,VKEY_PERIOD=190,VKEY_SLASH=191
};

BBGlfwGame *BBGlfwGame::_glfwGame;

BBGlfwGame::BBGlfwGame():_updatePeriod(0),_nextUpdate(0),_swapInterval( CFG_GLFW_SWAP_INTERVAL ){
	_glfwGame=this;
}

//***** BBGame *****

void Init_GL_Exts();

int glfwGraphicsSeq=0;

void BBGlfwGame::SetUpdateRate( int updateRate ){
	BBGame::SetUpdateRate( updateRate );
	if( _updateRate ) _updatePeriod=1.0/_updateRate;
	_nextUpdate=0;
}

int BBGlfwGame::Millisecs(){
	return int( glfwGetTime()*1000.0 );
}

bool BBGlfwGame::PollJoystick( int port,Array<Float> joyx,Array<Float> joyy,Array<Float> joyz,Array<bool> buttons ){

	int joy=GLFW_JOYSTICK_1+port;
	if( !glfwGetJoystickParam( joy,GLFW_PRESENT ) ) return false;

	//read axes
	float axes[6];
	memset( axes,0,sizeof(axes) );
	int n_axes=glfwGetJoystickPos( joy,axes,6 );
	joyx[0]=axes[0];joyy[0]=axes[1];joyz[0]=axes[2];
	joyx[1]=axes[3];joyy[1]=axes[4];joyz[1]=axes[5];
	
	//read buttons
	unsigned char buts[32];
	memset( buts,0,sizeof(buts) );
	int n_buts=glfwGetJoystickButtons( joy,buts,32 );
	if( n_buts>12 ){
		for( int i=0;i<8;++i ) buttons[i]=(buts[i]==GLFW_PRESS);
		for( int i=0;i<4;++i ) buttons[i+8]=(buts[n_buts-4+i]==GLFW_PRESS);
		for( int i=0;i<n_buts-12;++i ) buttons[i+12]=(buts[i+8]==GLFW_PRESS);
	}else{
		for( int i=0;i<n_buts;++i ) buttons[i]=(buts[i]=-GLFW_PRESS);
	}
	
	//kludges for device type!
	if( n_axes==5 && n_buts==14 ){
		//XBOX_360?
		joyx[1]=axes[4];
		joyy[1]=-axes[3];
	}else if( n_axes==4 && n_buts==18 ){
		//My Saitek?
		joyy[1]=-joyz[0];
	}
	
	//enough!
	return true;
}

void BBGlfwGame::OpenUrl( String url ){
#if _WIN32
	ShellExecute( HWND_DESKTOP,"open",url.ToCString<char>(),0,0,SW_SHOWNORMAL );
#elif __APPLE__
	if( CFURLRef cfurl=CFURLCreateWithBytes( 0,url.ToCString<UInt8>(),url.Length(),kCFStringEncodingASCII,0 ) ){
		LSOpenCFURLRef( cfurl,0 );
		CFRelease( cfurl );
	}
#elif __linux
	system( ( String( "xdg-open \"" )+url+"\"" ).ToCString<char>() );
#endif
}

void BBGlfwGame::SetMouseVisible( bool visible ){
	if( visible ){
		glfwEnable( GLFW_MOUSE_CURSOR );
	}else{
		glfwDisable( GLFW_MOUSE_CURSOR );
	}
}

String BBGlfwGame::PathToFilePath( String path ){
	if( !path.StartsWith( "monkey:" ) ){
		return path;
	}else if( path.StartsWith( "monkey://data/" ) ){
		return String("./data/")+path.Slice(14);
	}else if( path.StartsWith( "monkey://internal/" ) ){
		return String("./internal/")+path.Slice(18);
	}else if( path.StartsWith( "monkey://external/" ) ){
		return String("./external/")+path.Slice(18);
	}
	return "";
}

unsigned char *BBGlfwGame::LoadImageData( String path,int *width,int *height,int *depth ){

	FILE *f=OpenFile( path,"rb" );
	if( !f ) return 0;
	
	unsigned char *data=stbi_load_from_file( f,width,height,depth,0 );
	fclose( f );
	
	if( data ) gc_ext_malloced( (*width)*(*height)*(*depth) );
	
	return data;
}

unsigned char *BBGlfwGame::LoadAudioData( String path,int *length,int *channels,int *format,int *hertz ){

	FILE *f=OpenFile( path,"rb" );
	if( !f ) return 0;
	
	unsigned char *data=0;
	
	if( path.ToLower().EndsWith( ".wav" ) ){
		data=LoadWAV( f,length,channels,format,hertz );
	}else if( path.ToLower().EndsWith( ".ogg" ) ){
		data=LoadOGG( f,length,channels,format,hertz );
	}
	fclose( f );
	
	if( data ) gc_ext_malloced( (*length)*(*channels)*(*format) );
	
	return data;
}

//glfw key to monkey key!
int BBGlfwGame::TransKey( int key ){

	if( key>='0' && key<='9' ) return key;
	if( key>='A' && key<='Z' ) return key;

	switch( key ){

	case ' ':return VKEY_SPACE;
	case ';':return VKEY_SEMICOLON;
	case '=':return VKEY_EQUALS;
	case ',':return VKEY_COMMA;
	case '-':return VKEY_MINUS;
	case '.':return VKEY_PERIOD;
	case '/':return VKEY_SLASH;
	case '~':return VKEY_TILDE;
	case '[':return VKEY_OPENBRACKET;
	case ']':return VKEY_CLOSEBRACKET;
	case '\"':return VKEY_QUOTES;
	case '\\':return VKEY_BACKSLASH;
	
	case '`':return VKEY_TILDE;
	case '\'':return VKEY_QUOTES;

	case GLFW_KEY_LSHIFT:
	case GLFW_KEY_RSHIFT:return VKEY_SHIFT;
	case GLFW_KEY_LCTRL:
	case GLFW_KEY_RCTRL:return VKEY_CONTROL;
	
//	case GLFW_KEY_LSHIFT:return VKEY_LSHIFT;
//	case GLFW_KEY_RSHIFT:return VKEY_RSHIFT;
//	case GLFW_KEY_LCTRL:return VKEY_LCONTROL;
//	case GLFW_KEY_RCTRL:return VKEY_RCONTROL;
	
	case GLFW_KEY_BACKSPACE:return VKEY_BACKSPACE;
	case GLFW_KEY_TAB:return VKEY_TAB;
	case GLFW_KEY_ENTER:return VKEY_ENTER;
	case GLFW_KEY_ESC:return VKEY_ESC;
	case GLFW_KEY_INSERT:return VKEY_INSERT;
	case GLFW_KEY_DEL:return VKEY_DELETE;
	case GLFW_KEY_PAGEUP:return VKEY_PAGEUP;
	case GLFW_KEY_PAGEDOWN:return VKEY_PAGEDOWN;
	case GLFW_KEY_HOME:return VKEY_HOME;
	case GLFW_KEY_END:return VKEY_END;
	case GLFW_KEY_UP:return VKEY_UP;
	case GLFW_KEY_DOWN:return VKEY_DOWN;
	case GLFW_KEY_LEFT:return VKEY_LEFT;
	case GLFW_KEY_RIGHT:return VKEY_RIGHT;
	
	case GLFW_KEY_KP_0:return VKEY_NUM0;
	case GLFW_KEY_KP_1:return VKEY_NUM1;
	case GLFW_KEY_KP_2:return VKEY_NUM2;
	case GLFW_KEY_KP_3:return VKEY_NUM3;
	case GLFW_KEY_KP_4:return VKEY_NUM4;
	case GLFW_KEY_KP_5:return VKEY_NUM5;
	case GLFW_KEY_KP_6:return VKEY_NUM6;
	case GLFW_KEY_KP_7:return VKEY_NUM7;
	case GLFW_KEY_KP_8:return VKEY_NUM8;
	case GLFW_KEY_KP_9:return VKEY_NUM9;
	case GLFW_KEY_KP_DIVIDE:return VKEY_NUMDIVIDE;
	case GLFW_KEY_KP_MULTIPLY:return VKEY_NUMMULTIPLY;
	case GLFW_KEY_KP_SUBTRACT:return VKEY_NUMSUBTRACT;
	case GLFW_KEY_KP_ADD:return VKEY_NUMADD;
	case GLFW_KEY_KP_DECIMAL:return VKEY_NUMDECIMAL;
    	
	case GLFW_KEY_F1:return VKEY_F1;
	case GLFW_KEY_F2:return VKEY_F2;
	case GLFW_KEY_F3:return VKEY_F3;
	case GLFW_KEY_F4:return VKEY_F4;
	case GLFW_KEY_F5:return VKEY_F5;
	case GLFW_KEY_F6:return VKEY_F6;
	case GLFW_KEY_F7:return VKEY_F7;
	case GLFW_KEY_F8:return VKEY_F8;
	case GLFW_KEY_F9:return VKEY_F9;
	case GLFW_KEY_F10:return VKEY_F10;
	case GLFW_KEY_F11:return VKEY_F11;
	case GLFW_KEY_F12:return VKEY_F12;
	}
	return 0;
}

//monkey key to special monkey char
int BBGlfwGame::KeyToChar( int key ){
	switch( key ){
	case VKEY_BACKSPACE:
	case VKEY_TAB:
	case VKEY_ENTER:
	case VKEY_ESC:
		return key;
	case VKEY_PAGEUP:
	case VKEY_PAGEDOWN:
	case VKEY_END:
	case VKEY_HOME:
	case VKEY_LEFT:
	case VKEY_UP:
	case VKEY_RIGHT:
	case VKEY_DOWN:
	case VKEY_INSERT:
		return key | 0x10000;
	case VKEY_DELETE:
		return 127;
	}
	return 0;
}

void BBGlfwGame::OnMouseButton( int button,int action ){
	switch( button ){
	case GLFW_MOUSE_BUTTON_LEFT:button=0;break;
	case GLFW_MOUSE_BUTTON_RIGHT:button=1;break;
	case GLFW_MOUSE_BUTTON_MIDDLE:button=2;break;
	default:return;
	}
	int x,y;
	glfwGetMousePos( &x,&y );
	switch( action ){
	case GLFW_PRESS:
		_glfwGame->MouseEvent( BBGameEvent::MouseDown,button,x,y );
		break;
	case GLFW_RELEASE:
		_glfwGame->MouseEvent( BBGameEvent::MouseUp,button,x,y );
		break;
	}
}

void BBGlfwGame::OnMousePos( int x,int y ){
	_game->MouseEvent( BBGameEvent::MouseMove,-1,x,y );
}

int BBGlfwGame::OnWindowClose(){
	_game->KeyEvent( BBGameEvent::KeyDown,0x1b0 );
	_game->KeyEvent( BBGameEvent::KeyUp,0x1b0 );
	return GL_FALSE;
}

void BBGlfwGame::OnKey( int key,int action ){

	key=TransKey( key );
	if( !key ) return;
	
	switch( action ){
	case GLFW_PRESS:
		_glfwGame->KeyEvent( BBGameEvent::KeyDown,key );
		if( int chr=KeyToChar( key ) ) _game->KeyEvent( BBGameEvent::KeyChar,chr );
		break;
	case GLFW_RELEASE:
		_glfwGame->KeyEvent( BBGameEvent::KeyUp,key );
		break;
	}
}

void BBGlfwGame::OnChar( int chr,int action ){

	switch( action ){
	case GLFW_PRESS:
		_glfwGame->KeyEvent( BBGameEvent::KeyChar,chr );
		break;
	}
}

void BBGlfwGame::SetGlfwWindow( int width,int height,int red,int green,int blue,int alpha,int depth,int stencil,bool fullscreen ){

	for( int i=0;i<=GLFW_KEY_LAST;++i ){
		int key=TransKey( i );
		if( key && glfwGetKey( i )==GLFW_PRESS ) KeyEvent( BBGameEvent::KeyUp,key );
	}

	GLFWvidmode desktopMode;
	glfwGetDesktopMode( &desktopMode );

	glfwCloseWindow();
	
	glfwOpenWindowHint( GLFW_REFRESH_RATE,60 );
	glfwOpenWindowHint( GLFW_WINDOW_NO_RESIZE,CFG_GLFW_WINDOW_RESIZABLE ? GL_FALSE : GL_TRUE );

	glfwOpenWindow( width,height,red,green,blue,alpha,depth,stencil,fullscreen ? GLFW_FULLSCREEN : GLFW_WINDOW );

	++glfwGraphicsSeq;

	if( !fullscreen ){	
		glfwSetWindowPos( (desktopMode.Width-width)/2,(desktopMode.Height-height)/2 );
		glfwSetWindowTitle( _STRINGIZE(CFG_GLFW_WINDOW_TITLE) );
	}

#if CFG_OPENGL_INIT_EXTENSIONS
	Init_GL_Exts();
#endif

	if( _swapInterval>=0 ) glfwSwapInterval( CFG_GLFW_SWAP_INTERVAL );

	glfwEnable( GLFW_KEY_REPEAT );
	glfwDisable( GLFW_AUTO_POLL_EVENTS );
	glfwSetKeyCallback( OnKey );
	glfwSetCharCallback( OnChar );
	glfwSetMouseButtonCallback( OnMouseButton );
	glfwSetMousePosCallback( OnMousePos );
	glfwSetWindowCloseCallback(	OnWindowClose );
}

Array<BBGlfwVideoMode*> BBGlfwGame::GetGlfwVideoModes(){
	GLFWvidmode modes[1024];
	int n=glfwGetVideoModes( modes,1024 );
	Array<BBGlfwVideoMode*> bbmodes( n );
	for( int i=0;i<n;++i ){
		bbmodes[i]=new BBGlfwVideoMode( modes[i].Width,modes[i].Height,modes[i].RedBits,modes[i].GreenBits,modes[i].BlueBits );
	}
	return bbmodes;
}

BBGlfwVideoMode *BBGlfwGame::GetGlfwDesktopMode(){
	GLFWvidmode mode;
	glfwGetDesktopMode( &mode );
	return new BBGlfwVideoMode( mode.Width,mode.Height,mode.RedBits,mode.GreenBits,mode.BlueBits );
}

int BBGlfwGame::GetDeviceWidth(){
	int width,height;
	glfwGetWindowSize( &width,&height );
	return width;
}

int BBGlfwGame::GetDeviceHeight(){
	int width,height;
	glfwGetWindowSize( &width,&height );
	return height;
}

void BBGlfwGame::SetDeviceWindow( int width,int height,int flags ){

	SetGlfwWindow( width,height,8,8,8,0,CFG_OPENGL_DEPTH_BUFFER_ENABLED ? 32 : 0,0,(flags&1)!=0 );
}

Array<BBDisplayMode*> BBGlfwGame::GetDisplayModes(){

	GLFWvidmode vmodes[1024];
	int n=glfwGetVideoModes( vmodes,1024 );
	Array<BBDisplayMode*> modes( n );
	for( int i=0;i<n;++i ) modes[i]=new BBDisplayMode( vmodes[i].Width,vmodes[i].Height );
	return modes;
}

BBDisplayMode *BBGlfwGame::GetDesktopMode(){

	GLFWvidmode vmode;
	glfwGetDesktopMode( &vmode );
	return new BBDisplayMode( vmode.Width,vmode.Height );
}

void BBGlfwGame::SetSwapInterval( int interval ){
	_swapInterval=interval;
	if( _swapInterval>=0 ) glfwSwapInterval( CFG_GLFW_SWAP_INTERVAL );
}

void BBGlfwGame::UpdateEvents(){
	if( _suspended ){
		glfwWaitEvents();
	}else{
		glfwPollEvents();
	}
	if( glfwGetWindowParam( GLFW_ACTIVE ) ){
		if( _suspended ){
			ResumeGame();
			_nextUpdate=0;
		}
	}else if( glfwGetWindowParam( GLFW_ICONIFIED ) || CFG_MOJO_AUTO_SUSPEND_ENABLED ){
		if( !_suspended ){
			SuspendGame();
			_nextUpdate=0;
		}
	}
}

void BBGlfwGame::Run(){

#if	CFG_GLFW_WINDOW_WIDTH && CFG_GLFW_WINDOW_HEIGHT

	SetGlfwWindow( CFG_GLFW_WINDOW_WIDTH,CFG_GLFW_WINDOW_HEIGHT,8,8,8,0,CFG_OPENGL_DEPTH_BUFFER_ENABLED ? 32 : 0,0,CFG_GLFW_WINDOW_FULLSCREEN );

#endif

	StartGame();
	
	while( glfwGetWindowParam( GLFW_OPENED ) ){
	
		RenderGame();
		glfwSwapBuffers();
		
		if( _nextUpdate ){
			double delay=_nextUpdate-glfwGetTime();
			if( delay>0 ) glfwSleep( delay );
		}
		
		//Update user events
		UpdateEvents();

		//App suspended?		
		if( _suspended ) continue;

		//'Go nuts' mode!
		if( !_updateRate ){
			UpdateGame();
			continue;
		}
		
		//Reset update timer?
		if( !_nextUpdate ) _nextUpdate=glfwGetTime();
		
		//Catch up updates...
		int i=0;
		for( ;i<4;++i ){
		
			UpdateGame();
			if( !_nextUpdate ) break;
			
			_nextUpdate+=_updatePeriod;
			
			if( _nextUpdate>glfwGetTime() ) break;
		}
		
		if( i==4 ) _nextUpdate=0;
	}
}



//***** monkeygame.h *****

class BBMonkeyGame : public BBGlfwGame{
public:

	static void Main( int args,const char *argv[] );
};

//***** monkeygame.cpp *****

#define _QUOTE(X) #X
#define _STRINGIZE(X) _QUOTE(X)

void BBMonkeyGame::Main( int argc,const char *argv[] ){

	if( !glfwInit() ){
		puts( "glfwInit failed" );
		exit(-1);
	}

	BBMonkeyGame *game=new BBMonkeyGame();
	
	try{
	
		bb_std_main( argc,argv );
		
	}catch( ThrowableObject *ex ){
	
		glfwTerminate();
		
		game->Die( ex );
		
		return;
	}

	if( game->Delegate() ) game->Run();
	
	glfwTerminate();
}


// GLFW mojo runtime.
//
// Copyright 2011 Mark Sibly, all rights reserved.
// No warranty implied; use at your own risk.

//***** gxtkGraphics.h *****

class gxtkSurface;

class gxtkGraphics : public Object{
public:

	enum{
		MAX_VERTS=1024,
		MAX_QUADS=(MAX_VERTS/4)
	};

	int width;
	int height;

	int colorARGB;
	float r,g,b,alpha;
	float ix,iy,jx,jy,tx,ty;
	bool tformed;

	float vertices[MAX_VERTS*5];
	unsigned short quadIndices[MAX_QUADS*6];

	int primType;
	int vertCount;
	gxtkSurface *primSurf;
	
	gxtkGraphics();
	
	void Flush();
	float *Begin( int type,int count,gxtkSurface *surf );
	
	//***** GXTK API *****
	virtual int Width();
	virtual int Height();
	
	virtual int  BeginRender();
	virtual void EndRender();
	virtual void DiscardGraphics();

	virtual gxtkSurface *LoadSurface( String path );
	virtual gxtkSurface *LoadSurface__UNSAFE__( gxtkSurface *surface,String path );
	virtual gxtkSurface *CreateSurface( int width,int height );
	
	virtual int Cls( float r,float g,float b );
	virtual int SetAlpha( float alpha );
	virtual int SetColor( float r,float g,float b );
	virtual int SetBlend( int blend );
	virtual int SetScissor( int x,int y,int w,int h );
	virtual int SetMatrix( float ix,float iy,float jx,float jy,float tx,float ty );
	
	virtual int DrawPoint( float x,float y );
	virtual int DrawRect( float x,float y,float w,float h );
	virtual int DrawLine( float x1,float y1,float x2,float y2 );
	virtual int DrawOval( float x1,float y1,float x2,float y2 );
	virtual int DrawPoly( Array<Float> verts );
	virtual int DrawPoly2( Array<Float> verts,gxtkSurface *surface,int srcx,int srcy );
	virtual int DrawSurface( gxtkSurface *surface,float x,float y );
	virtual int DrawSurface2( gxtkSurface *surface,float x,float y,int srcx,int srcy,int srcw,int srch );
	
	virtual int ReadPixels( Array<int> pixels,int x,int y,int width,int height,int offset,int pitch );
	virtual int WritePixels2( gxtkSurface *surface,Array<int> pixels,int x,int y,int width,int height,int offset,int pitch );
};

class gxtkSurface : public Object{
public:
	unsigned char *data;
	int width;
	int height;
	int depth;
	int format;
	int seq;
	
	GLuint texture;
	float uscale;
	float vscale;
	
	gxtkSurface();
	
	void SetData( unsigned char *data,int width,int height,int depth );
	void SetSubData( int x,int y,int w,int h,unsigned *src,int pitch );
	void Bind();
	
	~gxtkSurface();
	
	//***** GXTK API *****
	virtual int Discard();
	virtual int Width();
	virtual int Height();
	virtual int Loaded();
	virtual bool OnUnsafeLoadComplete();
};

//***** gxtkGraphics.cpp *****

#ifndef GL_BGRA
#define GL_BGRA  0x80e1
#endif

#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812f
#endif

#ifndef GL_GENERATE_MIPMAP
#define GL_GENERATE_MIPMAP 0x8191
#endif

static int Pow2Size( int n ){
	int i=1;
	while( i<n ) i+=i;
	return i;
}

gxtkGraphics::gxtkGraphics(){

	width=height=0;
#ifdef _glfw3_h_
	glfwGetWindowSize( BBGlfwGame::GlfwGame()->GetGLFWwindow(),&width,&height );
#else
	glfwGetWindowSize( &width,&height );
#endif
	
	if( CFG_OPENGL_GLES20_ENABLED ) return;
	
	for( int i=0;i<MAX_QUADS;++i ){
		quadIndices[i*6  ]=(short)(i*4);
		quadIndices[i*6+1]=(short)(i*4+1);
		quadIndices[i*6+2]=(short)(i*4+2);
		quadIndices[i*6+3]=(short)(i*4);
		quadIndices[i*6+4]=(short)(i*4+2);
		quadIndices[i*6+5]=(short)(i*4+3);
	}
}

void gxtkGraphics::Flush(){
	if( !vertCount ) return;

	if( primSurf ){
		glEnable( GL_TEXTURE_2D );
		primSurf->Bind();
	}
		
	switch( primType ){
	case 1:
		glDrawArrays( GL_POINTS,0,vertCount );
		break;
	case 2:
		glDrawArrays( GL_LINES,0,vertCount );
		break;
	case 3:
		glDrawArrays( GL_TRIANGLES,0,vertCount );
		break;
	case 4:
		glDrawElements( GL_TRIANGLES,vertCount/4*6,GL_UNSIGNED_SHORT,quadIndices );
		break;
	default:
		for( int j=0;j<vertCount;j+=primType ){
			glDrawArrays( GL_TRIANGLE_FAN,j,primType );
		}
		break;
	}

	if( primSurf ){
		glDisable( GL_TEXTURE_2D );
	}

	vertCount=0;
}

float *gxtkGraphics::Begin( int type,int count,gxtkSurface *surf ){
	if( primType!=type || primSurf!=surf || vertCount+count>MAX_VERTS ){
		Flush();
		primType=type;
		primSurf=surf;
	}
	float *vp=vertices+vertCount*5;
	vertCount+=count;
	return vp;
}

//***** GXTK API *****

int gxtkGraphics::Width(){
	return width;
}

int gxtkGraphics::Height(){
	return height;
}

int gxtkGraphics::BeginRender(){

	width=height=0;
#ifdef _glfw3_h_
	glfwGetWindowSize( BBGlfwGame::GlfwGame()->GetGLFWwindow(),&width,&height );
#else
	glfwGetWindowSize( &width,&height );
#endif
	
	if( CFG_OPENGL_GLES20_ENABLED ) return 0;
	
	glViewport( 0,0,width,height );

	glMatrixMode( GL_PROJECTION );
	glLoadIdentity();
	glOrtho( 0,width,height,0,-1,1 );
	glMatrixMode( GL_MODELVIEW );
	glLoadIdentity();
	
	glEnableClientState( GL_VERTEX_ARRAY );
	glVertexPointer( 2,GL_FLOAT,20,&vertices[0] );	
	
	glEnableClientState( GL_TEXTURE_COORD_ARRAY );
	glTexCoordPointer( 2,GL_FLOAT,20,&vertices[2] );
	
	glEnableClientState( GL_COLOR_ARRAY );
	glColorPointer( 4,GL_UNSIGNED_BYTE,20,&vertices[4] );
	
	glEnable( GL_BLEND );
	glBlendFunc( GL_ONE,GL_ONE_MINUS_SRC_ALPHA );
	
	glDisable( GL_TEXTURE_2D );
	
	vertCount=0;
	
	return 1;
}

void gxtkGraphics::EndRender(){
	if( !CFG_OPENGL_GLES20_ENABLED ) Flush();
}

void gxtkGraphics::DiscardGraphics(){
}

int gxtkGraphics::Cls( float r,float g,float b ){
	vertCount=0;

	glClearColor( r/255.0f,g/255.0f,b/255.0f,1 );
	glClear( GL_COLOR_BUFFER_BIT );

	return 0;
}

int gxtkGraphics::SetAlpha( float alpha ){
	this->alpha=alpha;
	
	int a=int(alpha*255);
	
	colorARGB=(a<<24) | (int(b*alpha)<<16) | (int(g*alpha)<<8) | int(r*alpha);
	
	return 0;
}

int gxtkGraphics::SetColor( float r,float g,float b ){
	this->r=r;
	this->g=g;
	this->b=b;

	int a=int(alpha*255);
	
	colorARGB=(a<<24) | (int(b*alpha)<<16) | (int(g*alpha)<<8) | int(r*alpha);
	
	return 0;
}

int gxtkGraphics::SetBlend( int blend ){

	Flush();
	
	switch( blend ){
	case 1:
		glBlendFunc( GL_ONE,GL_ONE );
		break;
	default:
		glBlendFunc( GL_ONE,GL_ONE_MINUS_SRC_ALPHA );
	}

	return 0;
}

int gxtkGraphics::SetScissor( int x,int y,int w,int h ){

	Flush();
	
	if( x!=0 || y!=0 || w!=Width() || h!=Height() ){
		glEnable( GL_SCISSOR_TEST );
		y=Height()-y-h;
		glScissor( x,y,w,h );
	}else{
		glDisable( GL_SCISSOR_TEST );
	}
	return 0;
}

int gxtkGraphics::SetMatrix( float ix,float iy,float jx,float jy,float tx,float ty ){

	tformed=(ix!=1 || iy!=0 || jx!=0 || jy!=1 || tx!=0 || ty!=0);

	this->ix=ix;this->iy=iy;this->jx=jx;this->jy=jy;this->tx=tx;this->ty=ty;

	return 0;
}

int gxtkGraphics::DrawPoint( float x,float y ){

	if( tformed ){
		float px=x;
		x=px * ix + y * jx + tx;
		y=px * iy + y * jy + ty;
	}
	
	float *vp=Begin( 1,1,0 );
	
	vp[0]=x;vp[1]=y;(int&)vp[4]=colorARGB;

	return 0;	
}
	
int gxtkGraphics::DrawLine( float x0,float y0,float x1,float y1 ){

	if( tformed ){
		float tx0=x0,tx1=x1;
		x0=tx0 * ix + y0 * jx + tx;y0=tx0 * iy + y0 * jy + ty;
		x1=tx1 * ix + y1 * jx + tx;y1=tx1 * iy + y1 * jy + ty;
	}
	
	float *vp=Begin( 2,2,0 );

	vp[0]=x0;vp[1]=y0;(int&)vp[4]=colorARGB;
	vp[5]=x1;vp[6]=y1;(int&)vp[9]=colorARGB;
	
	return 0;
}

int gxtkGraphics::DrawRect( float x,float y,float w,float h ){

	float x0=x,x1=x+w,x2=x+w,x3=x;
	float y0=y,y1=y,y2=y+h,y3=y+h;

	if( tformed ){
		float tx0=x0,tx1=x1,tx2=x2,tx3=x3;
		x0=tx0 * ix + y0 * jx + tx;y0=tx0 * iy + y0 * jy + ty;
		x1=tx1 * ix + y1 * jx + tx;y1=tx1 * iy + y1 * jy + ty;
		x2=tx2 * ix + y2 * jx + tx;y2=tx2 * iy + y2 * jy + ty;
		x3=tx3 * ix + y3 * jx + tx;y3=tx3 * iy + y3 * jy + ty;
	}
	
	float *vp=Begin( 4,4,0 );

	vp[0 ]=x0;vp[1 ]=y0;(int&)vp[4 ]=colorARGB;
	vp[5 ]=x1;vp[6 ]=y1;(int&)vp[9 ]=colorARGB;
	vp[10]=x2;vp[11]=y2;(int&)vp[14]=colorARGB;
	vp[15]=x3;vp[16]=y3;(int&)vp[19]=colorARGB;

	return 0;
}

int gxtkGraphics::DrawOval( float x,float y,float w,float h ){
	
	float xr=w/2.0f;
	float yr=h/2.0f;

	int n;
	if( tformed ){
		float dx_x=xr * ix;
		float dx_y=xr * iy;
		float dx=sqrtf( dx_x*dx_x+dx_y*dx_y );
		float dy_x=yr * jx;
		float dy_y=yr * jy;
		float dy=sqrtf( dy_x*dy_x+dy_y*dy_y );
		n=(int)( dx+dy );
	}else{
		n=(int)( abs( xr )+abs( yr ) );
	}
	
	if( n<12 ){
		n=12;
	}else if( n>MAX_VERTS ){
		n=MAX_VERTS;
	}else{
		n&=~3;
	}

	float x0=x+xr,y0=y+yr;
	
	float *vp=Begin( n,n,0 );

	for( int i=0;i<n;++i ){
	
		float th=i * 6.28318531f / n;

		float px=x0+cosf( th ) * xr;
		float py=y0-sinf( th ) * yr;
		
		if( tformed ){
			float ppx=px;
			px=ppx * ix + py * jx + tx;
			py=ppx * iy + py * jy + ty;
		}
		
		vp[0]=px;vp[1]=py;(int&)vp[4]=colorARGB;
		vp+=5;
	}
	
	return 0;
}

int gxtkGraphics::DrawPoly( Array<Float> verts ){

	int n=verts.Length()/2;
	if( n<1 || n>MAX_VERTS ) return 0;
	
	float *vp=Begin( n,n,0 );
	
	for( int i=0;i<n;++i ){
		int j=i*2;
		if( tformed ){
			vp[0]=verts[j] * ix + verts[j+1] * jx + tx;
			vp[1]=verts[j] * iy + verts[j+1] * jy + ty;
		}else{
			vp[0]=verts[j];
			vp[1]=verts[j+1];
		}
		(int&)vp[4]=colorARGB;
		vp+=5;
	}

	return 0;
}

int gxtkGraphics::DrawPoly2( Array<Float> verts,gxtkSurface *surface,int srcx,int srcy ){

	int n=verts.Length()/4;
	if( n<1 || n>MAX_VERTS ) return 0;
		
	float *vp=Begin( n,n,surface );
	
	for( int i=0;i<n;++i ){
		int j=i*4;
		if( tformed ){
			vp[0]=verts[j] * ix + verts[j+1] * jx + tx;
			vp[1]=verts[j] * iy + verts[j+1] * jy + ty;
		}else{
			vp[0]=verts[j];
			vp[1]=verts[j+1];
		}
		vp[2]=(srcx+verts[j+2])*surface->uscale;
		vp[3]=(srcy+verts[j+3])*surface->vscale;
		(int&)vp[4]=colorARGB;
		vp+=5;
	}
	
	return 0;
}

int gxtkGraphics::DrawSurface( gxtkSurface *surf,float x,float y ){
	
	float w=surf->Width();
	float h=surf->Height();
	float x0=x,x1=x+w,x2=x+w,x3=x;
	float y0=y,y1=y,y2=y+h,y3=y+h;
	float u0=0,u1=w*surf->uscale;
	float v0=0,v1=h*surf->vscale;

	if( tformed ){
		float tx0=x0,tx1=x1,tx2=x2,tx3=x3;
		x0=tx0 * ix + y0 * jx + tx;y0=tx0 * iy + y0 * jy + ty;
		x1=tx1 * ix + y1 * jx + tx;y1=tx1 * iy + y1 * jy + ty;
		x2=tx2 * ix + y2 * jx + tx;y2=tx2 * iy + y2 * jy + ty;
		x3=tx3 * ix + y3 * jx + tx;y3=tx3 * iy + y3 * jy + ty;
	}
	
	float *vp=Begin( 4,4,surf );
	
	vp[0 ]=x0;vp[1 ]=y0;vp[2 ]=u0;vp[3 ]=v0;(int&)vp[4 ]=colorARGB;
	vp[5 ]=x1;vp[6 ]=y1;vp[7 ]=u1;vp[8 ]=v0;(int&)vp[9 ]=colorARGB;
	vp[10]=x2;vp[11]=y2;vp[12]=u1;vp[13]=v1;(int&)vp[14]=colorARGB;
	vp[15]=x3;vp[16]=y3;vp[17]=u0;vp[18]=v1;(int&)vp[19]=colorARGB;
	
	return 0;
}

int gxtkGraphics::DrawSurface2( gxtkSurface *surf,float x,float y,int srcx,int srcy,int srcw,int srch ){
	
	float w=srcw;
	float h=srch;
	float x0=x,x1=x+w,x2=x+w,x3=x;
	float y0=y,y1=y,y2=y+h,y3=y+h;
	float u0=srcx*surf->uscale,u1=(srcx+srcw)*surf->uscale;
	float v0=srcy*surf->vscale,v1=(srcy+srch)*surf->vscale;

	if( tformed ){
		float tx0=x0,tx1=x1,tx2=x2,tx3=x3;
		x0=tx0 * ix + y0 * jx + tx;y0=tx0 * iy + y0 * jy + ty;
		x1=tx1 * ix + y1 * jx + tx;y1=tx1 * iy + y1 * jy + ty;
		x2=tx2 * ix + y2 * jx + tx;y2=tx2 * iy + y2 * jy + ty;
		x3=tx3 * ix + y3 * jx + tx;y3=tx3 * iy + y3 * jy + ty;
	}
	
	float *vp=Begin( 4,4,surf );
	
	vp[0 ]=x0;vp[1 ]=y0;vp[2 ]=u0;vp[3 ]=v0;(int&)vp[4 ]=colorARGB;
	vp[5 ]=x1;vp[6 ]=y1;vp[7 ]=u1;vp[8 ]=v0;(int&)vp[9 ]=colorARGB;
	vp[10]=x2;vp[11]=y2;vp[12]=u1;vp[13]=v1;(int&)vp[14]=colorARGB;
	vp[15]=x3;vp[16]=y3;vp[17]=u0;vp[18]=v1;(int&)vp[19]=colorARGB;
	
	return 0;
}
	
int gxtkGraphics::ReadPixels( Array<int> pixels,int x,int y,int width,int height,int offset,int pitch ){

	Flush();

	unsigned *p=(unsigned*)malloc(width*height*4);

	glReadPixels( x,this->height-y-height,width,height,GL_BGRA,GL_UNSIGNED_BYTE,p );
	
	for( int py=0;py<height;++py ){
		memcpy( &pixels[offset+py*pitch],&p[(height-py-1)*width],width*4 );
	}
	
	free( p );
	
	return 0;
}

int gxtkGraphics::WritePixels2( gxtkSurface *surface,Array<int> pixels,int x,int y,int width,int height,int offset,int pitch ){

	Flush();
	
	surface->SetSubData( x,y,width,height,(unsigned*)&pixels[offset],pitch );
	
	return 0;
}

//***** gxtkSurface *****

gxtkSurface::gxtkSurface():data(0),width(0),height(0),depth(0),format(0),seq(-1),texture(0),uscale(0),vscale(0){
}

gxtkSurface::~gxtkSurface(){
	Discard();
}

int gxtkSurface::Discard(){
	if( seq==glfwGraphicsSeq ){
		glDeleteTextures( 1,&texture );
		seq=-1;
	}
	if( data ){
		free( data );
		data=0;
	}
	return 0;
}

int gxtkSurface::Width(){
	return width;
}

int gxtkSurface::Height(){
	return height;
}

int gxtkSurface::Loaded(){
	return 1;
}

//Careful! Can't call any GL here as it may be executing off-thread.
//
void gxtkSurface::SetData( unsigned char *data,int width,int height,int depth ){

	this->data=data;
	this->width=width;
	this->height=height;
	this->depth=depth;
	
	unsigned char *p=data;
	int n=width*height;
	
	switch( depth ){
	case 1:
		format=GL_LUMINANCE;
		break;
	case 2:
		format=GL_LUMINANCE_ALPHA;
		if( data ){
			while( n-- ){	//premultiply alpha
				p[0]=p[0]*p[1]/255;
				p+=2;
			}
		}
		break;
	case 3:
		format=GL_RGB;
		break;
	case 4:
		format=GL_RGBA;
		if( data ){
			while( n-- ){	//premultiply alpha
				p[0]=p[0]*p[3]/255;
				p[1]=p[1]*p[3]/255;
				p[2]=p[2]*p[3]/255;
				p+=4;
			}
		}
		break;
	}
}

void gxtkSurface::SetSubData( int x,int y,int w,int h,unsigned *src,int pitch ){
	if( format!=GL_RGBA ) return;
	
	if( !data ) data=(unsigned char*)malloc( width*height*4 );
	
	unsigned *dst=(unsigned*)data+y*width+x;
	
	for( int py=0;py<h;++py ){
		unsigned *d=dst+py*width;
		unsigned *s=src+py*pitch;
		for( int px=0;px<w;++px ){
			unsigned p=*s++;
			unsigned a=p>>24;
			*d++=(a<<24) | ((p>>0&0xff)*a/255<<16) | ((p>>8&0xff)*a/255<<8) | ((p>>16&0xff)*a/255);
		}
	}
	
	if( seq==glfwGraphicsSeq ){
		glBindTexture( GL_TEXTURE_2D,texture );
		glPixelStorei( GL_UNPACK_ALIGNMENT,1 );
		if( width==pitch ){
			glTexSubImage2D( GL_TEXTURE_2D,0,x,y,w,h,format,GL_UNSIGNED_BYTE,dst );
		}else{
			for( int py=0;py<h;++py ){
				glTexSubImage2D( GL_TEXTURE_2D,0,x,y+py,w,1,format,GL_UNSIGNED_BYTE,dst+py*width );
			}
		}
	}
}

void gxtkSurface::Bind(){

	if( !glfwGraphicsSeq ) return;
	
	if( seq==glfwGraphicsSeq ){
		glBindTexture( GL_TEXTURE_2D,texture );
		return;
	}
	
	seq=glfwGraphicsSeq;
	
	glGenTextures( 1,&texture );
	glBindTexture( GL_TEXTURE_2D,texture );
	
	if( CFG_MOJO_IMAGE_FILTERING_ENABLED ){
		glTexParameteri( GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR );
		glTexParameteri( GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR );
	}else{
		glTexParameteri( GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST );
		glTexParameteri( GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST );
	}

	glTexParameteri( GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE );
	glTexParameteri( GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE );

	int texwidth=width;
	int texheight=height;
	
	glTexImage2D( GL_TEXTURE_2D,0,format,texwidth,texheight,0,format,GL_UNSIGNED_BYTE,0 );
	if( glGetError()!=GL_NO_ERROR ){
		texwidth=Pow2Size( width );
		texheight=Pow2Size( height );
		glTexImage2D( GL_TEXTURE_2D,0,format,texwidth,texheight,0,format,GL_UNSIGNED_BYTE,0 );
	}
	
	uscale=1.0/texwidth;
	vscale=1.0/texheight;
	
	if( data ){
		glPixelStorei( GL_UNPACK_ALIGNMENT,1 );
		glTexSubImage2D( GL_TEXTURE_2D,0,0,0,width,height,format,GL_UNSIGNED_BYTE,data );
	}
}

bool gxtkSurface::OnUnsafeLoadComplete(){
	Bind();
	return true;
}

gxtkSurface *gxtkGraphics::LoadSurface__UNSAFE__( gxtkSurface *surface,String path ){
	int width,height,depth;
	unsigned char *data=BBGlfwGame::GlfwGame()->LoadImageData( path,&width,&height,&depth );
	if( !data ) return 0;
	surface->SetData( data,width,height,depth );
	return surface;
}

gxtkSurface *gxtkGraphics::LoadSurface( String path ){
	gxtkSurface *surf=LoadSurface__UNSAFE__( new gxtkSurface(),path );
	if( !surf ) return 0;
	surf->Bind();
	return surf;
}

gxtkSurface *gxtkGraphics::CreateSurface( int width,int height ){
	gxtkSurface *surf=new gxtkSurface();
	surf->SetData( 0,width,height,4 );
	surf->Bind();
	return surf;
}

//***** gxtkAudio.h *****

class gxtkSample;

class gxtkChannel{
public:
	ALuint source;
	gxtkSample *sample;
	int flags;
	int state;
	
	int AL_Source();
};

class gxtkAudio : public Object{
public:
	ALCdevice *alcDevice;
	ALCcontext *alcContext;
	gxtkChannel channels[33];

	gxtkAudio();

	virtual void mark();

	//***** GXTK API *****
	virtual int Suspend();
	virtual int Resume();

	virtual gxtkSample *LoadSample__UNSAFE__( gxtkSample *sample,String path );
	virtual gxtkSample *LoadSample( String path );
	virtual int PlaySample( gxtkSample *sample,int channel,int flags );

	virtual int StopChannel( int channel );
	virtual int PauseChannel( int channel );
	virtual int ResumeChannel( int channel );
	virtual int ChannelState( int channel );
	virtual int SetVolume( int channel,float volume );
	virtual int SetPan( int channel,float pan );
	virtual int SetRate( int channel,float rate );
	
	virtual int PlayMusic( String path,int flags );
	virtual int StopMusic();
	virtual int PauseMusic();
	virtual int ResumeMusic();
	virtual int MusicState();
	virtual int SetMusicVolume( float volume );
};

class gxtkSample : public Object{
public:
	ALuint al_buffer;

	gxtkSample();
	gxtkSample( ALuint buf );
	~gxtkSample();
	
	void SetBuffer( ALuint buf );
	
	//***** GXTK API *****
	virtual int Discard();
};

//***** gxtkAudio.cpp *****

static std::vector<ALuint> discarded;

static void FlushDiscarded( gxtkAudio *audio ){

	if( !discarded.size() ) return;
	
	for( int i=0;i<33;++i ){
		gxtkChannel *chan=&audio->channels[i];
		if( chan->state ){
			int state=0;
			alGetSourcei( chan->source,AL_SOURCE_STATE,&state );
			if( state==AL_STOPPED ) alSourcei( chan->source,AL_BUFFER,0 );
		}
	}
	
	std::vector<ALuint> out;
	
	for( int i=0;i<discarded.size();++i ){
		ALuint buf=discarded[i];
		alDeleteBuffers( 1,&buf );
		ALenum err=alGetError();
		if( err==AL_NO_ERROR ){
//			printf( "alDeleteBuffers OK!\n" );fflush( stdout );
		}else{
//			printf( "alDeleteBuffers failed...\n" );fflush( stdout );
			out.push_back( buf );
		}
	}
	discarded=out;
}

int gxtkChannel::AL_Source(){
	if( !source ) alGenSources( 1,&source );
	return source;
}

gxtkAudio::gxtkAudio(){

	if( alcDevice=alcOpenDevice( 0 ) ){
		if( alcContext=alcCreateContext( alcDevice,0 ) ){
			if( alcMakeContextCurrent( alcContext ) ){
				//alc all go!
			}else{
				bbPrint( "OpenAl error: alcMakeContextCurrent failed" );
			}
		}else{
			bbPrint( "OpenAl error: alcCreateContext failed" );
		}
	}else{
		bbPrint( "OpenAl error: alcOpenDevice failed" );
	}

	alDistanceModel( AL_NONE );
	
	memset( channels,0,sizeof(channels) );
}

void gxtkAudio::mark(){
	for( int i=0;i<33;++i ){
		gxtkChannel *chan=&channels[i];
		if( chan->state!=0 ){
			int state=0;
			alGetSourcei( chan->source,AL_SOURCE_STATE,&state );
			if( state!=AL_STOPPED ) gc_mark( chan->sample );
		}
	}
}

int gxtkAudio::Suspend(){
	for( int i=0;i<33;++i ){
		gxtkChannel *chan=&channels[i];
		if( chan->state==1 ){
			int state=0;
			alGetSourcei( chan->source,AL_SOURCE_STATE,&state );
			if( state==AL_PLAYING ) alSourcePause( chan->source );
		}
	}
	return 0;
}

int gxtkAudio::Resume(){
	for( int i=0;i<33;++i ){
		gxtkChannel *chan=&channels[i];
		if( chan->state==1 ){
			int state=0;
			alGetSourcei( chan->source,AL_SOURCE_STATE,&state );
			if( state==AL_PAUSED ) alSourcePlay( chan->source );
		}
	}
	return 0;
}

gxtkSample *gxtkAudio::LoadSample__UNSAFE__( gxtkSample *sample,String path ){

	int length=0;
	int channels=0;
	int format=0;
	int hertz=0;
	unsigned char *data=BBGlfwGame::GlfwGame()->LoadAudioData( path,&length,&channels,&format,&hertz );
	if( !data ) return 0;
	
	int al_format=0;
	if( format==1 && channels==1 ){
		al_format=AL_FORMAT_MONO8;
	}else if( format==1 && channels==2 ){
		al_format=AL_FORMAT_STEREO8;
	}else if( format==2 && channels==1 ){
		al_format=AL_FORMAT_MONO16;
	}else if( format==2 && channels==2 ){
		al_format=AL_FORMAT_STEREO16;
	}
	
	int size=length*channels*format;
	
	ALuint al_buffer;
	alGenBuffers( 1,&al_buffer );
	alBufferData( al_buffer,al_format,data,size,hertz );
	free( data );
	
	sample->SetBuffer( al_buffer );
	return sample;
}

gxtkSample *gxtkAudio::LoadSample( String path ){

	FlushDiscarded( this );

	return LoadSample__UNSAFE__( new gxtkSample(),path );
}

int gxtkAudio::PlaySample( gxtkSample *sample,int channel,int flags ){

	FlushDiscarded( this );
	
	gxtkChannel *chan=&channels[channel];
	
	chan->AL_Source();
	
	alSourceStop( chan->source );
	alSourcei( chan->source,AL_BUFFER,sample->al_buffer );
	alSourcei( chan->source,AL_LOOPING,flags ? 1 : 0 );
	alSourcePlay( chan->source );
	
	gc_assign( chan->sample,sample );

	chan->flags=flags;
	chan->state=1;

	return 0;
}

int gxtkAudio::StopChannel( int channel ){
	gxtkChannel *chan=&channels[channel];

	if( chan->state!=0 ){
		alSourceStop( chan->source );
		chan->state=0;
	}
	return 0;
}

int gxtkAudio::PauseChannel( int channel ){
	gxtkChannel *chan=&channels[channel];

	if( chan->state==1 ){
		int state=0;
		alGetSourcei( chan->source,AL_SOURCE_STATE,&state );
		if( state==AL_STOPPED ){
			chan->state=0;
		}else{
			alSourcePause( chan->source );
			chan->state=2;
		}
	}
	return 0;
}

int gxtkAudio::ResumeChannel( int channel ){
	gxtkChannel *chan=&channels[channel];

	if( chan->state==2 ){
		alSourcePlay( chan->source );
		chan->state=1;
	}
	return 0;
}

int gxtkAudio::ChannelState( int channel ){
	gxtkChannel *chan=&channels[channel];
	
	if( chan->state==1 ){
		int state=0;
		alGetSourcei( chan->source,AL_SOURCE_STATE,&state );
		if( state==AL_STOPPED ) chan->state=0;
	}
	return chan->state;
}

int gxtkAudio::SetVolume( int channel,float volume ){
	gxtkChannel *chan=&channels[channel];

	alSourcef( chan->AL_Source(),AL_GAIN,volume );
	return 0;
}

int gxtkAudio::SetPan( int channel,float pan ){
	gxtkChannel *chan=&channels[channel];
	
	float x=sinf( pan ),y=0,z=-cosf( pan );
	alSource3f( chan->AL_Source(),AL_POSITION,x,y,z );
	return 0;
}

int gxtkAudio::SetRate( int channel,float rate ){
	gxtkChannel *chan=&channels[channel];

	alSourcef( chan->AL_Source(),AL_PITCH,rate );
	return 0;
}

int gxtkAudio::PlayMusic( String path,int flags ){
	StopMusic();
	
	gxtkSample *music=LoadSample( path );
	if( !music ) return -1;
	
	PlaySample( music,32,flags );
	return 0;
}

int gxtkAudio::StopMusic(){
	StopChannel( 32 );
	return 0;
}

int gxtkAudio::PauseMusic(){
	PauseChannel( 32 );
	return 0;
}

int gxtkAudio::ResumeMusic(){
	ResumeChannel( 32 );
	return 0;
}

int gxtkAudio::MusicState(){
	return ChannelState( 32 );
}

int gxtkAudio::SetMusicVolume( float volume ){
	SetVolume( 32,volume );
	return 0;
}

gxtkSample::gxtkSample():
al_buffer(0){
}

gxtkSample::gxtkSample( ALuint buf ):
al_buffer(buf){
}

gxtkSample::~gxtkSample(){
	puts( "Discarding sample" );
	Discard();
}

void gxtkSample::SetBuffer( ALuint buf ){
	al_buffer=buf;
}

int gxtkSample::Discard(){
	if( al_buffer ){
		discarded.push_back( al_buffer );
		al_buffer=0;
	}
	return 0;
}


// ***** thread.h *****

#if __cplusplus_winrt

using namespace Windows::System::Threading;

#endif

class BBThread : public Object{
public:
	Object *result;
	
	BBThread();
	~BBThread();
	
	virtual void Start();
	virtual bool IsRunning();
	virtual Object *Result();
	virtual void SetResult( Object *result );
	
	virtual void Run__UNSAFE__();
	
	virtual void Wait();
	
private:

	enum{
		INIT=0,
		RUNNING=1,
		FINISHED=2
	};

	
	int _state;
	Object *_result;
	
#if __cplusplus_winrt

	friend class Launcher;

	class Launcher{
	
		friend class BBThread;
		BBThread *_thread;
		
		Launcher( BBThread *thread ):_thread(thread){
		}
		
		public:
		void operator()( IAsyncAction ^operation ){
			_thread->Run__UNSAFE__();
			_thread->_state=FINISHED;
		} 
	};

#elif _WIN32

	DWORD _id;
	HANDLE _handle;
	
	static DWORD WINAPI run( void *p );
	
#else

	pthread_t _handle;
	
	static void *run( void *p );
	
#endif

};

// ***** thread.cpp *****

BBThread::BBThread():_result( 0 ),_state( INIT ){
}

BBThread::~BBThread(){
	Wait();
}

bool BBThread::IsRunning(){
	return _state==RUNNING;
}

void BBThread::SetResult( Object *result ){
	_result=result;
}

Object *BBThread::Result(){
	return _result;
}

void BBThread::Run__UNSAFE__(){
}

#if __cplusplus_winrt

void BBThread::Start(){
	if( _state==RUNNING ) return;
	
	if( _state==FINISHED ) {}

	_result=0;
	
	_state=RUNNING;
	
	Launcher launcher( this );
	
	auto handler=ref new WorkItemHandler( launcher );
	
	ThreadPool::RunAsync( handler );
}

void BBThread::Wait(){
//	exit( -1 );
}

#elif _WIN32

void BBThread::Start(){
	if( _state==RUNNING ) return;
	
	if( _state==FINISHED ) CloseHandle( _handle );

	_state=RUNNING;

	_handle=CreateThread( 0,0,run,this,0,&_id );
	
//	_handle=CreateThread( 0,0,run,this,CREATE_SUSPENDED,&_id );
//	SetThreadPriority( _handle,THREAD_PRIORITY_ABOVE_NORMAL );
//	ResumeThread( _handle );
}

void BBThread::Wait(){
	if( _state==INIT ) return;

	WaitForSingleObject( _handle,INFINITE );
	CloseHandle( _handle );

	_state=INIT;
}

DWORD WINAPI BBThread::run( void *p ){
	BBThread *thread=(BBThread*)p;

	thread->Run__UNSAFE__();
	
	thread->_state=FINISHED;
	return 0;
}

#else

void BBThread::Start(){
	if( _state==RUNNING ) return;
	
	if( _state==FINISHED ) pthread_join( _handle,0 );

	_result=0;
		
	_state=RUNNING;
	
	pthread_create( &_handle,0,run,this );
}

void BBThread::Wait(){
	if( _state==INIT ) return;
	
	pthread_join( _handle,0 );
	
	_state=INIT;
}

void *BBThread::run( void *p ){
	BBThread *thread=(BBThread*)p;

	thread->Run__UNSAFE__();

	thread->_state=FINISHED;
	return 0;
}

#endif

class c_BoolObject;
class c_IntObject;
class c_FloatObject;
class c_StringObject;
class c_Deque;
class c_IntDeque;
class c_Deque2;
class c_FloatDeque;
class c_Deque3;
class c_StringDeque;
class c_List;
class c_IntList;
class c_Node;
class c_HeadNode;
class c_List2;
class c_FloatList;
class c_Node2;
class c_HeadNode2;
class c_List3;
class c_StringList;
class c_Node3;
class c_HeadNode3;
class c_Enumerator;
class c_Set;
class c_IntSet;
class c_Map;
class c_IntMap;
class c_Set2;
class c_FloatSet;
class c_Map2;
class c_FloatMap;
class c_Set3;
class c_StringSet;
class c_Map3;
class c_StringMap;
class c_Stack;
class c_IntStack;
class c_Stack2;
class c_FloatStack;
class c_Stack3;
class c_StringStack;
class c_Color;
class c_ImmutableColor;
class c_GraphicsContext;
class c_Vec2;
class c_VEntity;
class c_Map4;
class c_StringMap2;
class c_Node4;
class c_MapKeys;
class c_KeyEnumerator;
class c_VShape;
class c_VRect;
class c_VCircle;
class c_VSprite;
class c_Image;
class c_ImageCache;
class c_Map5;
class c_StringMap3;
class c_Node5;
class c_Frame;
class c_Exception;
class c_Enumerator2;
class c_Enumerator3;
class c_Enumerator4;
class c_Enumerator5;
class c_BackwardsList;
class c_Enumerator6;
class c_BackwardsList2;
class c_BackwardsList3;
class c_BackwardsEnumerator;
class c_BackwardsEnumerator2;
class c_BackwardsEnumerator3;
class c_Node6;
class c_MapKeys2;
class c_MapValues;
class c_NodeEnumerator;
class c_Node7;
class c_MapKeys3;
class c_MapValues2;
class c_NodeEnumerator2;
class c_Node8;
class c_MapKeys4;
class c_MapValues3;
class c_NodeEnumerator3;
class c_MapValues4;
class c_NodeEnumerator4;
class c_MapKeys5;
class c_MapValues5;
class c_NodeEnumerator5;
class c_KeyEnumerator2;
class c_KeyEnumerator3;
class c_KeyEnumerator4;
class c_KeyEnumerator5;
class c_ValueEnumerator;
class c_ValueEnumerator2;
class c_ValueEnumerator3;
class c_ValueEnumerator4;
class c_ValueEnumerator5;
class c_Enumerator7;
class c_BackwardsStack;
class c_Enumerator8;
class c_BackwardsStack2;
class c_Enumerator9;
class c_BackwardsStack3;
class c_BackwardsEnumerator4;
class c_BackwardsEnumerator5;
class c_BackwardsEnumerator6;
class c_ArrayObject;
class c_ArrayObject2;
class c_ArrayObject3;
class c_ClassInfo;
class c_R63;
class c_R64;
class c_R70;
class c_R80;
class c_R90;
class c_R99;
class c_R121;
class c_R124;
class c_R146;
class c_R149;
class c_R171;
class c_R174;
class c_R175;
class c_R209;
class c_R214;
class c_R225;
class c_R228;
class c_R262;
class c_R267;
class c_R278;
class c_R281;
class c_R315;
class c_R321;
class c_R332;
class c_R335;
class c_R342;
class c_R353;
class c_R355;
class c_R381;
class c_R384;
class c_R395;
class c_R397;
class c_R423;
class c_R426;
class c_R437;
class c_R439;
class c_R465;
class c_R468;
class c_R505;
class c_R510;
class c_R547;
class c_R552;
class c_R589;
class c_R595;
class c_R601;
class c_R651;
class c_R665;
class c_R705;
class c_R722;
class c_R748;
class c_R751;
class c_R766;
class c_R771;
class c_R777;
class c_R789;
class c_R809;
class c_R821;
class c_R843;
class c_R850;
class c_R857;
class c_R864;
class c_R871;
class c_R876;
class c_R883;
class c_R888;
class c_R893;
class c_R900;
class c_R907;
class c_R914;
class c_R929;
class c_R934;
class c_R939;
class c_R945;
class c_R960;
class c_R965;
class c_R970;
class c_R976;
class c_R991;
class c_R996;
class c_R1001;
class c_R1007;
class c_R1012;
class c_R1018;
class c_R1024;
class c_R1030;
class c_R1036;
class c_R1042;
class c_R1048;
class c_R1054;
class c_R1060;
class c_R1067;
class c_R1072;
class c_R1079;
class c_R1084;
class c_R1091;
class c_R1096;
class c_R1103;
class c_R1110;
class c_R1117;
class c_R1122;
class c_R1127;
class c_ConstInfo;
class c_GlobalInfo;
class c_R59;
class c_FunctionInfo;
class c_R17;
class c_R18;
class c_R19;
class c_R20;
class c_R21;
class c_R22;
class c_R23;
class c_R24;
class c_R25;
class c_R26;
class c_R27;
class c_R28;
class c_R29;
class c_R30;
class c_R31;
class c_R32;
class c_R33;
class c_R34;
class c_R35;
class c_R36;
class c_R37;
class c_R38;
class c_R39;
class c_R40;
class c_R41;
class c_R42;
class c_R43;
class c_R44;
class c_R45;
class c_R46;
class c_R47;
class c_R48;
class c_R49;
class c_R50;
class c_R51;
class c_R52;
class c_R53;
class c_R54;
class c_R55;
class c_R56;
class c_R57;
class c_R58;
class c_R60;
class c_R61;
class c_R62;
class c__GetClass;
class c___GetClass;
class c_App;
class c_VsatApp;
class c_GameDelegate;
class c_VScene;
class c_VActionEventHandler;
class c_vvv;
class c_DummyScene;
class c_Stack4;
class c_FieldInfo;
class c_Stack5;
class c_Stack6;
class c_MethodInfo;
class c_Stack7;
class c_Stack8;
class c_R65;
class c_R67;
class c_R68;
class c_R66;
class c_R69;
class c_R71;
class c_R74;
class c_R75;
class c_R76;
class c_R77;
class c_R78;
class c_R72;
class c_R73;
class c_R79;
class c_R81;
class c_R84;
class c_R85;
class c_R86;
class c_R87;
class c_R88;
class c_R82;
class c_R83;
class c_R89;
class c_R91;
class c_R95;
class c_R96;
class c_R97;
class c_R92;
class c_R93;
class c_R94;
class c_R98;
class c_R115;
class c_R116;
class c_R117;
class c_R118;
class c_R119;
class c_R102;
class c_R103;
class c_R104;
class c_R105;
class c_R106;
class c_R107;
class c_R108;
class c_R109;
class c_R110;
class c_R111;
class c_R112;
class c_R113;
class c_R114;
class c_R120;
class c_R100;
class c_R101;
class c_R122;
class c_R123;
class c_R140;
class c_R141;
class c_R142;
class c_R143;
class c_R144;
class c_R127;
class c_R128;
class c_R129;
class c_R130;
class c_R131;
class c_R132;
class c_R133;
class c_R134;
class c_R135;
class c_R136;
class c_R137;
class c_R138;
class c_R139;
class c_R145;
class c_R125;
class c_R126;
class c_R147;
class c_R148;
class c_R165;
class c_R166;
class c_R167;
class c_R168;
class c_R169;
class c_R152;
class c_R153;
class c_R154;
class c_R155;
class c_R156;
class c_R157;
class c_R158;
class c_R159;
class c_R160;
class c_R161;
class c_R162;
class c_R163;
class c_R164;
class c_R170;
class c_R150;
class c_R151;
class c_R172;
class c_R173;
class c_R208;
class c_R178;
class c_R179;
class c_R180;
class c_R181;
class c_R182;
class c_R183;
class c_R184;
class c_R185;
class c_R186;
class c_R187;
class c_R188;
class c_R189;
class c_R190;
class c_R191;
class c_R192;
class c_R193;
class c_R194;
class c_R195;
class c_R196;
class c_R197;
class c_R198;
class c_R199;
class c_R200;
class c_R201;
class c_R202;
class c_R203;
class c_R204;
class c_R205;
class c_R206;
class c_R207;
class c_R176;
class c_R177;
class c_R211;
class c_R212;
class c_R210;
class c_R213;
class c_R220;
class c_R221;
class c_R222;
class c_R216;
class c_R217;
class c_R218;
class c_R219;
class c_R223;
class c_R215;
class c_R224;
class c_R227;
class c_R226;
class c_R261;
class c_R231;
class c_R232;
class c_R233;
class c_R234;
class c_R235;
class c_R236;
class c_R237;
class c_R238;
class c_R239;
class c_R240;
class c_R241;
class c_R242;
class c_R243;
class c_R244;
class c_R245;
class c_R246;
class c_R247;
class c_R248;
class c_R249;
class c_R250;
class c_R251;
class c_R252;
class c_R253;
class c_R254;
class c_R255;
class c_R256;
class c_R257;
class c_R258;
class c_R259;
class c_R260;
class c_R229;
class c_R230;
class c_R264;
class c_R265;
class c_R263;
class c_R266;
class c_R273;
class c_R274;
class c_R275;
class c_R269;
class c_R270;
class c_R271;
class c_R272;
class c_R276;
class c_R268;
class c_R277;
class c_R280;
class c_R279;
class c_R314;
class c_R284;
class c_R285;
class c_R286;
class c_R287;
class c_R288;
class c_R289;
class c_R290;
class c_R291;
class c_R292;
class c_R293;
class c_R294;
class c_R295;
class c_R296;
class c_R297;
class c_R298;
class c_R299;
class c_R300;
class c_R301;
class c_R302;
class c_R303;
class c_R304;
class c_R305;
class c_R306;
class c_R307;
class c_R308;
class c_R309;
class c_R310;
class c_R311;
class c_R312;
class c_R313;
class c_R282;
class c_R283;
class c_R317;
class c_R318;
class c_R319;
class c_R316;
class c_R320;
class c_R327;
class c_R328;
class c_R329;
class c_R323;
class c_R324;
class c_R325;
class c_R326;
class c_R330;
class c_R322;
class c_R331;
class c_R334;
class c_R333;
class c_R339;
class c_R340;
class c_R337;
class c_R338;
class c_R336;
class c_R341;
class c_R351;
class c_R344;
class c_R345;
class c_R346;
class c_R347;
class c_R348;
class c_R349;
class c_R350;
class c_R343;
class c_R352;
class c_R354;
class c_R379;
class c_R356;
class c_R357;
class c_R358;
class c_R359;
class c_R360;
class c_R361;
class c_R362;
class c_R363;
class c_R364;
class c_R365;
class c_R366;
class c_R367;
class c_R368;
class c_R369;
class c_R370;
class c_R371;
class c_R372;
class c_R373;
class c_R374;
class c_R375;
class c_R376;
class c_R377;
class c_R378;
class c_R380;
class c_R382;
class c_R383;
class c_R393;
class c_R386;
class c_R387;
class c_R388;
class c_R389;
class c_R390;
class c_R391;
class c_R392;
class c_R385;
class c_R394;
class c_R396;
class c_R421;
class c_R398;
class c_R399;
class c_R400;
class c_R401;
class c_R402;
class c_R403;
class c_R404;
class c_R405;
class c_R406;
class c_R407;
class c_R408;
class c_R409;
class c_R410;
class c_R411;
class c_R412;
class c_R413;
class c_R414;
class c_R415;
class c_R416;
class c_R417;
class c_R418;
class c_R419;
class c_R420;
class c_R422;
class c_R424;
class c_R425;
class c_R435;
class c_R428;
class c_R429;
class c_R430;
class c_R431;
class c_R432;
class c_R433;
class c_R434;
class c_R427;
class c_R436;
class c_R438;
class c_R463;
class c_R440;
class c_R441;
class c_R442;
class c_R443;
class c_R444;
class c_R445;
class c_R446;
class c_R447;
class c_R448;
class c_R449;
class c_R450;
class c_R451;
class c_R452;
class c_R453;
class c_R454;
class c_R455;
class c_R456;
class c_R457;
class c_R458;
class c_R459;
class c_R460;
class c_R461;
class c_R462;
class c_R464;
class c_R466;
class c_R467;
class c_R497;
class c_R498;
class c_R499;
class c_R471;
class c_R472;
class c_R473;
class c_R474;
class c_R475;
class c_R476;
class c_R477;
class c_R478;
class c_R479;
class c_R480;
class c_R481;
class c_R482;
class c_R483;
class c_R484;
class c_R485;
class c_R486;
class c_R487;
class c_R488;
class c_R489;
class c_R490;
class c_R491;
class c_R492;
class c_R493;
class c_R494;
class c_R495;
class c_R496;
class c_R500;
class c_R501;
class c_R502;
class c_R503;
class c_R504;
class c_R469;
class c_R470;
class c_R507;
class c_R508;
class c_R506;
class c_R509;
class c_R539;
class c_R540;
class c_R541;
class c_R513;
class c_R514;
class c_R515;
class c_R516;
class c_R517;
class c_R518;
class c_R519;
class c_R520;
class c_R521;
class c_R522;
class c_R523;
class c_R524;
class c_R525;
class c_R526;
class c_R527;
class c_R528;
class c_R529;
class c_R530;
class c_R531;
class c_R532;
class c_R533;
class c_R534;
class c_R535;
class c_R536;
class c_R537;
class c_R538;
class c_R542;
class c_R543;
class c_R544;
class c_R545;
class c_R546;
class c_R511;
class c_R512;
class c_R549;
class c_R550;
class c_R548;
class c_R551;
class c_R581;
class c_R582;
class c_R583;
class c_R555;
class c_R556;
class c_R557;
class c_R558;
class c_R559;
class c_R560;
class c_R561;
class c_R562;
class c_R563;
class c_R564;
class c_R565;
class c_R566;
class c_R567;
class c_R568;
class c_R569;
class c_R570;
class c_R571;
class c_R572;
class c_R573;
class c_R574;
class c_R575;
class c_R576;
class c_R577;
class c_R578;
class c_R579;
class c_R580;
class c_R584;
class c_R585;
class c_R586;
class c_R587;
class c_R588;
class c_R553;
class c_R554;
class c_R591;
class c_R592;
class c_R593;
class c_R590;
class c_R594;
class c_R596;
class c_R597;
class c_R598;
class c_R599;
class c_R600;
class c_R606;
class c_R607;
class c_R608;
class c_R609;
class c_R610;
class c_R611;
class c_R612;
class c_R613;
class c_R614;
class c_R615;
class c_R616;
class c_R617;
class c_R618;
class c_R619;
class c_R620;
class c_R621;
class c_R622;
class c_R623;
class c_R624;
class c_R625;
class c_R626;
class c_R602;
class c_R603;
class c_R604;
class c_R605;
class c_R630;
class c_R631;
class c_R632;
class c_R633;
class c_R634;
class c_R635;
class c_R636;
class c_R637;
class c_R638;
class c_R639;
class c_R640;
class c_R641;
class c_R642;
class c_R643;
class c_R644;
class c_R645;
class c_R646;
class c_R647;
class c_R648;
class c_R649;
class c_R627;
class c_R628;
class c_R629;
class c_R650;
class c_R655;
class c_R656;
class c_R657;
class c_R658;
class c_R659;
class c_R660;
class c_R661;
class c_R662;
class c_R663;
class c_R664;
class c_R652;
class c_R653;
class c_R654;
class c_R666;
class c_R667;
class c_R670;
class c_R672;
class c_R673;
class c_R674;
class c_R675;
class c_R676;
class c_R677;
class c_R678;
class c_R679;
class c_R680;
class c_R681;
class c_R682;
class c_R683;
class c_R684;
class c_R685;
class c_R686;
class c_R687;
class c_R688;
class c_R689;
class c_R690;
class c_R691;
class c_R692;
class c_R693;
class c_R671;
class c_R694;
class c_R695;
class c_R696;
class c_R697;
class c_R698;
class c_R699;
class c_R700;
class c_R701;
class c_R702;
class c_R703;
class c_R668;
class c_R669;
class c_R704;
class c_R706;
class c_R707;
class c_R708;
class c_R709;
class c_R710;
class c_R711;
class c_R712;
class c_R713;
class c_R714;
class c_R715;
class c_R716;
class c_R717;
class c_R718;
class c_R719;
class c_R720;
class c_R721;
class c_R746;
class c_R723;
class c_R724;
class c_R725;
class c_R726;
class c_R727;
class c_R728;
class c_R729;
class c_R730;
class c_R731;
class c_R732;
class c_R733;
class c_R734;
class c_R735;
class c_R736;
class c_R737;
class c_R738;
class c_R739;
class c_R740;
class c_R741;
class c_R742;
class c_R743;
class c_R744;
class c_R745;
class c_R747;
class c_R749;
class c_R750;
class c_R759;
class c_R760;
class c_R761;
class c_R762;
class c_R763;
class c_R764;
class c_R753;
class c_R754;
class c_R755;
class c_R756;
class c_R757;
class c_R758;
class c_R752;
class c_R765;
class c_R769;
class c_R768;
class c_R767;
class c_R770;
class c_R775;
class c_R773;
class c_R774;
class c_R772;
class c_R776;
class c_R778;
class c_R779;
class c_R780;
class c_R781;
class c_R782;
class c_R783;
class c_R784;
class c_R785;
class c_R786;
class c_R787;
class c_R788;
class c_R790;
class c_R793;
class c_R794;
class c_R795;
class c_R796;
class c_R797;
class c_R798;
class c_R799;
class c_R800;
class c_R801;
class c_R802;
class c_R803;
class c_R804;
class c_R805;
class c_R806;
class c_R807;
class c_R791;
class c_R792;
class c_R808;
class c_R810;
class c_R812;
class c_R813;
class c_R814;
class c_R815;
class c_R816;
class c_R817;
class c_R818;
class c_R819;
class c_R811;
class c_R820;
class c_R822;
class c_R823;
class c_R824;
class c_R825;
class c_R841;
class c_R827;
class c_R828;
class c_R829;
class c_R830;
class c_R831;
class c_R832;
class c_R833;
class c_R834;
class c_R835;
class c_R836;
class c_R837;
class c_R838;
class c_R839;
class c_R840;
class c_R826;
class c_R842;
class c_R847;
class c_R848;
class c_R845;
class c_R846;
class c_R844;
class c_R849;
class c_R854;
class c_R855;
class c_R852;
class c_R853;
class c_R851;
class c_R856;
class c_R861;
class c_R862;
class c_R859;
class c_R860;
class c_R858;
class c_R863;
class c_R868;
class c_R869;
class c_R866;
class c_R867;
class c_R865;
class c_R870;
class c_R874;
class c_R873;
class c_R872;
class c_R875;
class c_R880;
class c_R881;
class c_R878;
class c_R879;
class c_R877;
class c_R882;
class c_R886;
class c_R885;
class c_R884;
class c_R887;
class c_R891;
class c_R890;
class c_R889;
class c_R892;
class c_R897;
class c_R898;
class c_R895;
class c_R896;
class c_R894;
class c_R899;
class c_R904;
class c_R905;
class c_R902;
class c_R903;
class c_R901;
class c_R906;
class c_R911;
class c_R912;
class c_R909;
class c_R910;
class c_R908;
class c_R913;
class c_R922;
class c_R923;
class c_R924;
class c_R925;
class c_R926;
class c_R927;
class c_R916;
class c_R917;
class c_R918;
class c_R919;
class c_R920;
class c_R921;
class c_R915;
class c_R928;
class c_R932;
class c_R931;
class c_R930;
class c_R933;
class c_R937;
class c_R936;
class c_R935;
class c_R938;
class c_R943;
class c_R941;
class c_R942;
class c_R940;
class c_R944;
class c_R953;
class c_R954;
class c_R955;
class c_R956;
class c_R957;
class c_R958;
class c_R947;
class c_R948;
class c_R949;
class c_R950;
class c_R951;
class c_R952;
class c_R946;
class c_R959;
class c_R963;
class c_R962;
class c_R961;
class c_R964;
class c_R968;
class c_R967;
class c_R966;
class c_R969;
class c_R974;
class c_R972;
class c_R973;
class c_R971;
class c_R975;
class c_R984;
class c_R985;
class c_R986;
class c_R987;
class c_R988;
class c_R989;
class c_R978;
class c_R979;
class c_R980;
class c_R981;
class c_R982;
class c_R983;
class c_R977;
class c_R990;
class c_R994;
class c_R993;
class c_R992;
class c_R995;
class c_R999;
class c_R998;
class c_R997;
class c_R1000;
class c_R1005;
class c_R1003;
class c_R1004;
class c_R1002;
class c_R1006;
class c_R1010;
class c_R1009;
class c_R1008;
class c_R1011;
class c_R1016;
class c_R1014;
class c_R1015;
class c_R1013;
class c_R1017;
class c_R1022;
class c_R1020;
class c_R1021;
class c_R1019;
class c_R1023;
class c_R1028;
class c_R1026;
class c_R1027;
class c_R1025;
class c_R1029;
class c_R1034;
class c_R1032;
class c_R1033;
class c_R1031;
class c_R1035;
class c_R1040;
class c_R1038;
class c_R1039;
class c_R1037;
class c_R1041;
class c_R1046;
class c_R1044;
class c_R1045;
class c_R1043;
class c_R1047;
class c_R1052;
class c_R1050;
class c_R1051;
class c_R1049;
class c_R1053;
class c_R1058;
class c_R1056;
class c_R1057;
class c_R1055;
class c_R1059;
class c_R1064;
class c_R1065;
class c_R1062;
class c_R1063;
class c_R1061;
class c_R1066;
class c_R1070;
class c_R1069;
class c_R1068;
class c_R1071;
class c_R1076;
class c_R1077;
class c_R1074;
class c_R1075;
class c_R1073;
class c_R1078;
class c_R1082;
class c_R1081;
class c_R1080;
class c_R1083;
class c_R1088;
class c_R1089;
class c_R1086;
class c_R1087;
class c_R1085;
class c_R1090;
class c_R1094;
class c_R1093;
class c_R1092;
class c_R1095;
class c_R1100;
class c_R1101;
class c_R1098;
class c_R1099;
class c_R1097;
class c_R1102;
class c_R1107;
class c_R1108;
class c_R1105;
class c_R1106;
class c_R1104;
class c_R1109;
class c_R1114;
class c_R1115;
class c_R1112;
class c_R1113;
class c_R1111;
class c_R1116;
class c_R1118;
class c_R1120;
class c_R1119;
class c_R1121;
class c_R1123;
class c_R1125;
class c_R1124;
class c_R1126;
class c_R1128;
class c_R1130;
class c_R1129;
class c_R1131;
class c_InputDevice;
class c_JoyState;
class c_DisplayMode;
class c_Map6;
class c_IntMap2;
class c_Stack9;
class c_Node9;
class c_BBGameEvent;
class c_VTransition;
class c_VFadeInLinear;
class c_List4;
class c_Node10;
class c_HeadNode4;
class c_FontCache;
class c_AngelFont;
class c_Map7;
class c_StringMap4;
class c_Node11;
class c_Char;
class c_KernPair;
class c_Map8;
class c_IntMap3;
class c_Map9;
class c_IntMap4;
class c_Node12;
class c_Node13;
class c_XMLError;
class c_XMLNode;
class c_XMLDoc;
class c_XMLStringBuffer;
class c_List5;
class c_Node14;
class c_HeadNode5;
class c_Map10;
class c_StringMap5;
class c_Node15;
class c_XMLAttributeQuery;
class c_XMLAttributeQueryItem;
class c_XMLAttribute;
class c_Map11;
class c_StringMap6;
class c_Node16;
class c_Enumerator10;
class c_VAction;
class c_VVec2Action;
class c_List6;
class c_Node17;
class c_HeadNode6;
class c_Enumerator11;
class c_Enumerator12;
class c_Tweener;
class c_LinearTween;
class c_EaseInQuad;
class c_EaseOutQuad;
class c_EaseInOutQuad;
class c_EaseInCubic;
class c_EaseOutCubic;
class c_EaseInOutCubic;
class c_EaseInQuart;
class c_EaseOutQuart;
class c_EaseInOutQuart;
class c_EaseInQuint;
class c_EaseOutQuint;
class c_EaseInOutQuint;
class c_EaseInSine;
class c_EaseOutSine;
class c_EaseInOutSine;
class c_EaseInExpo;
class c_EaseOutExpo;
class c_EaseInOutExpo;
class c_EaseInCirc;
class c_EaseOutCirc;
class c_EaseInOutCirc;
class c_EaseInBack;
class c_EaseOutBack;
class c_EaseInOutBack;
class c_EaseInBounce;
class c_EaseOutBounce;
class c_EaseInOutBounce;
class c_EaseInElastic;
class c_EaseOutElastic;
class c_EaseInOutElastic;
class c_BoolObject : public Object{
	public:
	bool m_value;
	c_BoolObject();
	c_BoolObject* m_new(bool);
	bool p_ToBool();
	bool p_Equals(c_BoolObject*);
	c_BoolObject* m_new2();
	void mark();
};
class c_IntObject : public Object{
	public:
	int m_value;
	c_IntObject();
	c_IntObject* m_new(int);
	c_IntObject* m_new2(Float);
	int p_ToInt();
	Float p_ToFloat();
	String p_ToString();
	bool p_Equals2(c_IntObject*);
	int p_Compare(c_IntObject*);
	c_IntObject* m_new3();
	void mark();
};
class c_FloatObject : public Object{
	public:
	Float m_value;
	c_FloatObject();
	c_FloatObject* m_new(int);
	c_FloatObject* m_new2(Float);
	int p_ToInt();
	Float p_ToFloat();
	String p_ToString();
	bool p_Equals3(c_FloatObject*);
	int p_Compare2(c_FloatObject*);
	c_FloatObject* m_new3();
	void mark();
};
class c_StringObject : public Object{
	public:
	String m_value;
	c_StringObject();
	c_StringObject* m_new(int);
	c_StringObject* m_new2(Float);
	c_StringObject* m_new3(String);
	String p_ToString();
	bool p_Equals4(c_StringObject*);
	int p_Compare3(c_StringObject*);
	c_StringObject* m_new4();
	void mark();
};
Object* bb_boxes_BoxBool(bool);
Object* bb_boxes_BoxInt(int);
Object* bb_boxes_BoxFloat(Float);
Object* bb_boxes_BoxString(String);
bool bb_boxes_UnboxBool(Object*);
int bb_boxes_UnboxInt(Object*);
Float bb_boxes_UnboxFloat(Object*);
String bb_boxes_UnboxString(Object*);
class c_Deque : public Object{
	public:
	Array<int > m__data;
	int m__capacity;
	int m__last;
	int m__first;
	c_Deque();
	c_Deque* m_new();
	c_Deque* m_new2(Array<int >);
	static int m_NIL;
	void p_Clear();
	int p_Length();
	bool p_IsEmpty();
	Array<int > p_ToArray();
	c_Enumerator2* p_ObjectEnumerator();
	int p_Get(int);
	void p_Set(int,int);
	void p_Grow();
	void p_PushFirst(int);
	void p_PushLast(int);
	int p_PopFirst();
	int p_PopLast();
	int p_First();
	int p_Last();
	void mark();
};
class c_IntDeque : public c_Deque{
	public:
	c_IntDeque();
	c_IntDeque* m_new();
	c_IntDeque* m_new2(Array<int >);
	void mark();
};
class c_Deque2 : public Object{
	public:
	Array<Float > m__data;
	int m__capacity;
	int m__last;
	int m__first;
	c_Deque2();
	c_Deque2* m_new();
	c_Deque2* m_new2(Array<Float >);
	static Float m_NIL;
	void p_Clear();
	int p_Length();
	bool p_IsEmpty();
	Array<Float > p_ToArray();
	c_Enumerator3* p_ObjectEnumerator();
	Float p_Get(int);
	void p_Set2(int,Float);
	void p_Grow();
	void p_PushFirst2(Float);
	void p_PushLast2(Float);
	Float p_PopFirst();
	Float p_PopLast();
	Float p_First();
	Float p_Last();
	void mark();
};
class c_FloatDeque : public c_Deque2{
	public:
	c_FloatDeque();
	c_FloatDeque* m_new();
	c_FloatDeque* m_new2(Array<Float >);
	void mark();
};
class c_Deque3 : public Object{
	public:
	Array<String > m__data;
	int m__capacity;
	int m__last;
	int m__first;
	c_Deque3();
	c_Deque3* m_new();
	c_Deque3* m_new2(Array<String >);
	static String m_NIL;
	void p_Clear();
	int p_Length();
	bool p_IsEmpty();
	Array<String > p_ToArray();
	c_Enumerator4* p_ObjectEnumerator();
	String p_Get(int);
	void p_Set3(int,String);
	void p_Grow();
	void p_PushFirst3(String);
	void p_PushLast3(String);
	String p_PopFirst();
	String p_PopLast();
	String p_First();
	String p_Last();
	void mark();
};
class c_StringDeque : public c_Deque3{
	public:
	c_StringDeque();
	c_StringDeque* m_new();
	c_StringDeque* m_new2(Array<String >);
	void mark();
};
class c_List : public Object{
	public:
	c_Node* m__head;
	c_List();
	c_List* m_new();
	c_Node* p_AddLast(int);
	c_List* m_new2(Array<int >);
	virtual bool p_Equals5(int,int);
	virtual int p_Compare4(int,int);
	int p_Count();
	c_Enumerator5* p_ObjectEnumerator();
	Array<int > p_ToArray();
	int p_Clear();
	bool p_IsEmpty();
	bool p_Contains(int);
	c_Node* p_FirstNode();
	c_Node* p_LastNode();
	int p_First();
	int p_Last();
	int p_RemoveFirst();
	int p_RemoveLast();
	c_Node* p_AddFirst(int);
	c_Node* p_Find(int,c_Node*);
	c_Node* p_Find2(int);
	c_Node* p_FindLast(int,c_Node*);
	c_Node* p_FindLast2(int);
	int p_RemoveEach(int);
	void p_Remove(int);
	void p_RemoveFirst2(int);
	void p_RemoveLast2(int);
	c_Node* p_InsertBefore(int,int);
	c_Node* p_InsertAfter(int,int);
	void p_InsertBeforeEach(int,int);
	void p_InsertAfterEach(int,int);
	c_BackwardsList* p_Backwards();
	int p_Sort(int);
	void mark();
};
class c_IntList : public c_List{
	public:
	c_IntList();
	c_IntList* m_new(Array<int >);
	bool p_Equals5(int,int);
	int p_Compare4(int,int);
	c_IntList* m_new2();
	void mark();
};
class c_Node : public Object{
	public:
	c_Node* m__succ;
	c_Node* m__pred;
	int m__data;
	c_Node();
	c_Node* m_new(c_Node*,c_Node*,int);
	c_Node* m_new2();
	int p_Remove2();
	int p_Value();
	virtual c_Node* p_GetNode();
	c_Node* p_NextNode();
	c_Node* p_PrevNode();
	void mark();
};
class c_HeadNode : public c_Node{
	public:
	c_HeadNode();
	c_HeadNode* m_new();
	c_Node* p_GetNode();
	void mark();
};
class c_List2 : public Object{
	public:
	c_Node2* m__head;
	c_List2();
	c_List2* m_new();
	c_Node2* p_AddLast2(Float);
	c_List2* m_new2(Array<Float >);
	virtual bool p_Equals6(Float,Float);
	virtual int p_Compare5(Float,Float);
	int p_Count();
	c_Enumerator6* p_ObjectEnumerator();
	Array<Float > p_ToArray();
	int p_Clear();
	bool p_IsEmpty();
	bool p_Contains2(Float);
	c_Node2* p_FirstNode();
	c_Node2* p_LastNode();
	Float p_First();
	Float p_Last();
	Float p_RemoveFirst();
	Float p_RemoveLast();
	c_Node2* p_AddFirst2(Float);
	c_Node2* p_Find3(Float,c_Node2*);
	c_Node2* p_Find4(Float);
	c_Node2* p_FindLast3(Float,c_Node2*);
	c_Node2* p_FindLast4(Float);
	int p_RemoveEach2(Float);
	void p_Remove3(Float);
	void p_RemoveFirst3(Float);
	void p_RemoveLast3(Float);
	c_Node2* p_InsertBefore2(Float,Float);
	c_Node2* p_InsertAfter2(Float,Float);
	void p_InsertBeforeEach2(Float,Float);
	void p_InsertAfterEach2(Float,Float);
	c_BackwardsList2* p_Backwards();
	int p_Sort(int);
	void mark();
};
class c_FloatList : public c_List2{
	public:
	c_FloatList();
	c_FloatList* m_new(Array<Float >);
	bool p_Equals6(Float,Float);
	int p_Compare5(Float,Float);
	c_FloatList* m_new2();
	void mark();
};
class c_Node2 : public Object{
	public:
	c_Node2* m__succ;
	c_Node2* m__pred;
	Float m__data;
	c_Node2();
	c_Node2* m_new(c_Node2*,c_Node2*,Float);
	c_Node2* m_new2();
	int p_Remove2();
	Float p_Value();
	virtual c_Node2* p_GetNode();
	c_Node2* p_NextNode();
	c_Node2* p_PrevNode();
	void mark();
};
class c_HeadNode2 : public c_Node2{
	public:
	c_HeadNode2();
	c_HeadNode2* m_new();
	c_Node2* p_GetNode();
	void mark();
};
class c_List3 : public Object{
	public:
	c_Node3* m__head;
	c_List3();
	c_List3* m_new();
	c_Node3* p_AddLast3(String);
	c_List3* m_new2(Array<String >);
	int p_Count();
	c_Enumerator* p_ObjectEnumerator();
	Array<String > p_ToArray();
	virtual bool p_Equals7(String,String);
	virtual int p_Compare6(String,String);
	int p_Clear();
	bool p_IsEmpty();
	bool p_Contains3(String);
	c_Node3* p_FirstNode();
	c_Node3* p_LastNode();
	String p_First();
	String p_Last();
	String p_RemoveFirst();
	String p_RemoveLast();
	c_Node3* p_AddFirst3(String);
	c_Node3* p_Find5(String,c_Node3*);
	c_Node3* p_Find6(String);
	c_Node3* p_FindLast5(String,c_Node3*);
	c_Node3* p_FindLast6(String);
	int p_RemoveEach3(String);
	void p_Remove4(String);
	void p_RemoveFirst4(String);
	void p_RemoveLast4(String);
	c_Node3* p_InsertBefore3(String,String);
	c_Node3* p_InsertAfter3(String,String);
	void p_InsertBeforeEach3(String,String);
	void p_InsertAfterEach3(String,String);
	c_BackwardsList3* p_Backwards();
	int p_Sort(int);
	void mark();
};
class c_StringList : public c_List3{
	public:
	c_StringList();
	c_StringList* m_new(Array<String >);
	String p_Join(String);
	bool p_Equals7(String,String);
	int p_Compare6(String,String);
	c_StringList* m_new2();
	void mark();
};
class c_Node3 : public Object{
	public:
	c_Node3* m__succ;
	c_Node3* m__pred;
	String m__data;
	c_Node3();
	c_Node3* m_new(c_Node3*,c_Node3*,String);
	c_Node3* m_new2();
	int p_Remove2();
	String p_Value();
	virtual c_Node3* p_GetNode();
	c_Node3* p_NextNode();
	c_Node3* p_PrevNode();
	void mark();
};
class c_HeadNode3 : public c_Node3{
	public:
	c_HeadNode3();
	c_HeadNode3* m_new();
	c_Node3* p_GetNode();
	void mark();
};
class c_Enumerator : public Object{
	public:
	c_List3* m__list;
	c_Node3* m__curr;
	c_Enumerator();
	c_Enumerator* m_new(c_List3*);
	c_Enumerator* m_new2();
	bool p_HasNext();
	String p_NextObject();
	void mark();
};
int bb_math_Sgn(int);
int bb_math_Abs(int);
int bb_math_Min(int,int);
int bb_math_Max(int,int);
int bb_math_Clamp(int,int,int);
Float bb_math_Sgn2(Float);
Float bb_math_Abs2(Float);
Float bb_math_Min2(Float,Float);
Float bb_math_Max2(Float,Float);
Float bb_math_Clamp2(Float,Float,Float);
extern int bb_random_Seed;
Float bb_random_Rnd();
Float bb_random_Rnd2(Float,Float);
Float bb_random_Rnd3(Float);
class c_Set : public Object{
	public:
	c_Map* m_map;
	c_Set();
	c_Set* m_new(c_Map*);
	c_Set* m_new2();
	int p_Clear();
	int p_Count();
	bool p_IsEmpty();
	bool p_Contains(int);
	int p_Insert(int);
	int p_Remove(int);
	c_KeyEnumerator2* p_ObjectEnumerator();
	void mark();
};
class c_IntSet : public c_Set{
	public:
	c_IntSet();
	c_IntSet* m_new();
	void mark();
};
class c_Map : public Object{
	public:
	c_Node6* m_root;
	c_Map();
	c_Map* m_new();
	virtual int p_Compare4(int,int)=0;
	int p_Clear();
	int p_Count();
	bool p_IsEmpty();
	c_Node6* p_FindNode(int);
	bool p_Contains(int);
	int p_RotateLeft(c_Node6*);
	int p_RotateRight(c_Node6*);
	int p_InsertFixup(c_Node6*);
	bool p_Set4(int,Object*);
	bool p_Add(int,Object*);
	bool p_Update(int,Object*);
	Object* p_Get(int);
	int p_DeleteFixup(c_Node6*,c_Node6*);
	int p_RemoveNode(c_Node6*);
	int p_Remove(int);
	c_MapKeys2* p_Keys();
	c_MapValues* p_Values();
	c_Node6* p_FirstNode();
	c_NodeEnumerator* p_ObjectEnumerator();
	bool p_Insert2(int,Object*);
	Object* p_ValueForKey(int);
	c_Node6* p_LastNode();
	void mark();
};
class c_IntMap : public c_Map{
	public:
	c_IntMap();
	c_IntMap* m_new();
	int p_Compare4(int,int);
	void mark();
};
class c_Set2 : public Object{
	public:
	c_Map2* m_map;
	c_Set2();
	c_Set2* m_new(c_Map2*);
	c_Set2* m_new2();
	int p_Clear();
	int p_Count();
	bool p_IsEmpty();
	bool p_Contains2(Float);
	int p_Insert3(Float);
	int p_Remove3(Float);
	c_KeyEnumerator3* p_ObjectEnumerator();
	void mark();
};
class c_FloatSet : public c_Set2{
	public:
	c_FloatSet();
	c_FloatSet* m_new();
	void mark();
};
class c_Map2 : public Object{
	public:
	c_Node7* m_root;
	c_Map2();
	c_Map2* m_new();
	virtual int p_Compare5(Float,Float)=0;
	int p_Clear();
	int p_Count();
	bool p_IsEmpty();
	c_Node7* p_FindNode2(Float);
	bool p_Contains2(Float);
	int p_RotateLeft2(c_Node7*);
	int p_RotateRight2(c_Node7*);
	int p_InsertFixup2(c_Node7*);
	bool p_Set5(Float,Object*);
	bool p_Add2(Float,Object*);
	bool p_Update2(Float,Object*);
	Object* p_Get2(Float);
	int p_DeleteFixup2(c_Node7*,c_Node7*);
	int p_RemoveNode2(c_Node7*);
	int p_Remove3(Float);
	c_MapKeys3* p_Keys();
	c_MapValues2* p_Values();
	c_Node7* p_FirstNode();
	c_NodeEnumerator2* p_ObjectEnumerator();
	bool p_Insert4(Float,Object*);
	Object* p_ValueForKey2(Float);
	c_Node7* p_LastNode();
	void mark();
};
class c_FloatMap : public c_Map2{
	public:
	c_FloatMap();
	c_FloatMap* m_new();
	int p_Compare5(Float,Float);
	void mark();
};
class c_Set3 : public Object{
	public:
	c_Map3* m_map;
	c_Set3();
	c_Set3* m_new(c_Map3*);
	c_Set3* m_new2();
	int p_Clear();
	int p_Count();
	bool p_IsEmpty();
	bool p_Contains3(String);
	int p_Insert5(String);
	int p_Remove4(String);
	c_KeyEnumerator4* p_ObjectEnumerator();
	void mark();
};
class c_StringSet : public c_Set3{
	public:
	c_StringSet();
	c_StringSet* m_new();
	void mark();
};
class c_Map3 : public Object{
	public:
	c_Node8* m_root;
	c_Map3();
	c_Map3* m_new();
	virtual int p_Compare6(String,String)=0;
	int p_Clear();
	int p_Count();
	bool p_IsEmpty();
	c_Node8* p_FindNode3(String);
	bool p_Contains3(String);
	int p_RotateLeft3(c_Node8*);
	int p_RotateRight3(c_Node8*);
	int p_InsertFixup3(c_Node8*);
	bool p_Set6(String,Object*);
	bool p_Add3(String,Object*);
	bool p_Update3(String,Object*);
	Object* p_Get3(String);
	int p_DeleteFixup3(c_Node8*,c_Node8*);
	int p_RemoveNode3(c_Node8*);
	int p_Remove4(String);
	c_MapKeys4* p_Keys();
	c_MapValues3* p_Values();
	c_Node8* p_FirstNode();
	c_NodeEnumerator3* p_ObjectEnumerator();
	bool p_Insert6(String,Object*);
	Object* p_ValueForKey3(String);
	c_Node8* p_LastNode();
	void mark();
};
class c_StringMap : public c_Map3{
	public:
	c_StringMap();
	c_StringMap* m_new();
	int p_Compare6(String,String);
	void mark();
};
class c_Stack : public Object{
	public:
	Array<int > m_data;
	int m_length;
	c_Stack();
	c_Stack* m_new();
	c_Stack* m_new2(Array<int >);
	virtual bool p_Equals5(int,int);
	virtual int p_Compare4(int,int);
	Array<int > p_ToArray();
	static int m_NIL;
	void p_Clear();
	void p_Length2(int);
	int p_Length();
	bool p_IsEmpty();
	bool p_Contains(int);
	void p_Push(int);
	void p_Push2(Array<int >,int,int);
	void p_Push3(Array<int >,int);
	int p_Pop();
	int p_Top();
	void p_Set(int,int);
	int p_Get(int);
	int p_Find7(int,int);
	int p_FindLast7(int,int);
	int p_FindLast2(int);
	void p_Insert7(int,int);
	void p_Remove(int);
	void p_RemoveFirst2(int);
	void p_RemoveLast2(int);
	void p_RemoveEach(int);
	bool p__Less(int,int,int);
	void p__Swap(int,int);
	bool p__Less2(int,int,int);
	bool p__Less3(int,int,int);
	void p__Sort(int,int,int);
	void p_Sort2(bool);
	c_Enumerator7* p_ObjectEnumerator();
	c_BackwardsStack* p_Backwards();
	void mark();
};
class c_IntStack : public c_Stack{
	public:
	c_IntStack();
	c_IntStack* m_new(Array<int >);
	bool p_Equals5(int,int);
	int p_Compare4(int,int);
	c_IntStack* m_new2();
	void mark();
};
class c_Stack2 : public Object{
	public:
	Array<Float > m_data;
	int m_length;
	c_Stack2();
	c_Stack2* m_new();
	c_Stack2* m_new2(Array<Float >);
	virtual bool p_Equals6(Float,Float);
	virtual int p_Compare5(Float,Float);
	Array<Float > p_ToArray();
	static Float m_NIL;
	void p_Clear();
	void p_Length2(int);
	int p_Length();
	bool p_IsEmpty();
	bool p_Contains2(Float);
	void p_Push4(Float);
	void p_Push5(Array<Float >,int,int);
	void p_Push6(Array<Float >,int);
	Float p_Pop();
	Float p_Top();
	void p_Set2(int,Float);
	Float p_Get(int);
	int p_Find8(Float,int);
	int p_FindLast8(Float,int);
	int p_FindLast4(Float);
	void p_Insert8(int,Float);
	void p_Remove(int);
	void p_RemoveFirst3(Float);
	void p_RemoveLast3(Float);
	void p_RemoveEach2(Float);
	bool p__Less(int,int,int);
	void p__Swap(int,int);
	bool p__Less22(int,Float,int);
	bool p__Less32(Float,int,int);
	void p__Sort(int,int,int);
	void p_Sort2(bool);
	c_Enumerator8* p_ObjectEnumerator();
	c_BackwardsStack2* p_Backwards();
	void mark();
};
class c_FloatStack : public c_Stack2{
	public:
	c_FloatStack();
	c_FloatStack* m_new(Array<Float >);
	bool p_Equals6(Float,Float);
	int p_Compare5(Float,Float);
	c_FloatStack* m_new2();
	void mark();
};
class c_Stack3 : public Object{
	public:
	Array<String > m_data;
	int m_length;
	c_Stack3();
	c_Stack3* m_new();
	c_Stack3* m_new2(Array<String >);
	Array<String > p_ToArray();
	virtual bool p_Equals7(String,String);
	virtual int p_Compare6(String,String);
	static String m_NIL;
	void p_Clear();
	void p_Length2(int);
	int p_Length();
	bool p_IsEmpty();
	bool p_Contains3(String);
	void p_Push7(String);
	void p_Push8(Array<String >,int,int);
	void p_Push9(Array<String >,int);
	String p_Pop();
	String p_Top();
	void p_Set3(int,String);
	String p_Get(int);
	int p_Find9(String,int);
	int p_FindLast9(String,int);
	int p_FindLast6(String);
	void p_Insert9(int,String);
	void p_Remove(int);
	void p_RemoveFirst4(String);
	void p_RemoveLast4(String);
	void p_RemoveEach3(String);
	bool p__Less(int,int,int);
	void p__Swap(int,int);
	bool p__Less23(int,String,int);
	bool p__Less33(String,int,int);
	void p__Sort(int,int,int);
	void p_Sort2(bool);
	c_Enumerator9* p_ObjectEnumerator();
	c_BackwardsStack3* p_Backwards();
	void mark();
};
class c_StringStack : public c_Stack3{
	public:
	c_StringStack();
	c_StringStack* m_new(Array<String >);
	String p_Join(String);
	bool p_Equals7(String,String);
	int p_Compare6(String,String);
	c_StringStack* m_new2();
	void mark();
};
class c_Color : public Object{
	public:
	Float m_red;
	Float m_green;
	Float m_blue;
	Float m_alpha;
	c_Color();
	virtual void p_RGB(int);
	int p_RGB2();
	c_Color* m_new(int);
	Float p_Red();
	virtual void p_Red2(Float);
	Float p_Green();
	virtual void p_Green2(Float);
	Float p_Blue();
	virtual void p_Blue2(Float);
	Float p_Alpha();
	virtual void p_Alpha2(Float);
	virtual void p_Set7(Float,Float,Float,Float);
	virtual void p_Set8(c_Color*);
	void p_Set9(int);
	c_Color* m_new2(Float,Float,Float,Float);
	c_Color* m_new3(c_Color*);
	c_Color* m_new4();
	static c_ImmutableColor* m_Black;
	static c_ImmutableColor* m_White;
	static c_ImmutableColor* m_PureRed;
	static c_ImmutableColor* m_PureGreen;
	static c_ImmutableColor* m_PureBlue;
	static c_ImmutableColor* m_Navy;
	static c_ImmutableColor* m_NewBlue;
	static c_ImmutableColor* m_Aqua;
	static c_ImmutableColor* m_Teal;
	static c_ImmutableColor* m_Olive;
	static c_ImmutableColor* m_NewGreen;
	static c_ImmutableColor* m_Lime;
	static c_ImmutableColor* m_Yellow;
	static c_ImmutableColor* m_Orange;
	static c_ImmutableColor* m_NewRed;
	static c_ImmutableColor* m_Maroon;
	static c_ImmutableColor* m_Fuchsia;
	static c_ImmutableColor* m_Purple;
	static c_ImmutableColor* m_Silver;
	static c_ImmutableColor* m_Gray;
	static c_ImmutableColor* m_NewBlack;
	virtual void p_Reset();
	int p_ARGB();
	virtual void p_Randomize();
	bool p_Equals8(c_Color*);
	String p_ToString();
	void p_Use();
	void p_UseWithoutAlpha();
	void mark();
};
class c_ImmutableColor : public c_Color{
	public:
	c_ImmutableColor();
	c_ImmutableColor* m_new();
	c_ImmutableColor* m_new2(int);
	c_ImmutableColor* m_new3(Float,Float,Float,Float);
	void p_CantChangeError();
	void p_Set7(Float,Float,Float,Float);
	void p_Set8(c_Color*);
	void p_Reset();
	void p_Randomize();
	void p_Red2(Float);
	void p_Green2(Float);
	void p_Blue2(Float);
	void p_Alpha2(Float);
	void p_RGB(int);
	void mark();
};
void bb_functions2_NoDefaultConstructorError(String);
class c_GraphicsContext : public Object{
	public:
	Float m_color_r;
	Float m_color_g;
	Float m_color_b;
	Float m_alpha;
	Float m_ix;
	Float m_jx;
	Float m_iy;
	Float m_jy;
	Float m_tx;
	Float m_ty;
	int m_tformed;
	int m_matDirty;
	int m_matrixSp;
	Array<Float > m_matrixStack;
	c_Image* m_defaultFont;
	c_Image* m_font;
	int m_firstChar;
	int m_blend;
	Float m_scissor_x;
	Float m_scissor_y;
	Float m_scissor_width;
	Float m_scissor_height;
	c_GraphicsContext();
	c_GraphicsContext* m_new();
	int p_Validate();
	void mark();
};
extern c_GraphicsContext* bb_graphics_context;
extern gxtkGraphics* bb_graphics_renderDevice;
int bb_graphics_SetColor(Float,Float,Float);
int bb_graphics_SetAlpha(Float);
class c_Vec2 : public Object{
	public:
	Float m_x;
	Float m_y;
	c_Vec2();
	c_Vec2* m_new(Float,Float);
	c_Vec2* m_new2(c_Vec2*);
	c_Vec2* m_new3();
	c_Vec2* p_Copy();
	static c_Vec2* m_FromPolar(Float,Float);
	c_Vec2* p_Set10(Float,Float);
	c_Vec2* p_Set11(c_Vec2*);
	c_Vec2* p_Add4(c_Vec2*);
	c_Vec2* p_Add5(Float,Float);
	c_Vec2* p_Sub(c_Vec2*);
	c_Vec2* p_Sub2(Float,Float);
	c_Vec2* p_Mul(Float);
	c_Vec2* p_Div(Float);
	Float p_Length();
	static c_Vec2* m_Mul(c_Vec2*,Float);
	void p_Length3(Float);
	c_Vec2* p_Normalize();
	Float p_Dot(c_Vec2*);
	void p_Limit(Float);
	Float p_LengthSquared();
	Float p_Angle();
	void p_Angle2(Float);
	void p_RotateLeft4();
	Float p_DistanceTo(Float,Float);
	Float p_DistanceTo2(c_Vec2*);
	bool p_Equals9(c_Vec2*);
	static Float m_Dot(c_Vec2*,c_Vec2*);
	Float p_ProjectOn(c_Vec2*);
	String p_ToString();
	static c_Vec2* m_Up();
	static c_Vec2* m_Down();
	static c_Vec2* m_Left();
	static c_Vec2* m_Right();
	static c_Vec2* m_Add(c_Vec2*,c_Vec2*);
	static c_Vec2* m_Sub(c_Vec2*,c_Vec2*);
	static c_Vec2* m_Div(c_Vec2*,Float);
	static Float m_AngleBetween(c_Vec2*,c_Vec2*);
	void mark();
};
class c_VEntity : public Object{
	public:
	c_StringMap2* m_attributes;
	c_Vec2* m_position;
	c_Vec2* m_scale;
	Float m_rotation;
	c_VEntity();
	void p_SetAttribute(String,String);
	void p_SetAttribute2(String,bool);
	String p_GetAttribute(String);
	bool p_HasAttribute(String);
	int p_NumberOfAttributes();
	c_StringMap2* p_GetAttributeMap();
	void p_SetAttributeMap(c_StringMap2*);
	void p_SetScale(Float);
	void p_ApplyTransform();
	virtual void p_Update4(Float);
	virtual void p_Render();
	c_VEntity* m_new();
	void mark();
};
class c_Map4 : public Object{
	public:
	c_Node4* m_root;
	c_Map4();
	c_Map4* m_new();
	virtual int p_Compare6(String,String)=0;
	int p_RotateLeft5(c_Node4*);
	int p_RotateRight4(c_Node4*);
	int p_InsertFixup4(c_Node4*);
	bool p_Set12(String,String);
	c_Node4* p_FindNode3(String);
	String p_Get3(String);
	bool p_Contains3(String);
	int p_Count();
	c_MapKeys* p_Keys();
	c_Node4* p_FirstNode();
	int p_Clear();
	bool p_IsEmpty();
	bool p_Add6(String,String);
	bool p_Update5(String,String);
	int p_DeleteFixup4(c_Node4*,c_Node4*);
	int p_RemoveNode4(c_Node4*);
	int p_Remove4(String);
	c_MapValues4* p_Values();
	c_NodeEnumerator4* p_ObjectEnumerator();
	bool p_Insert10(String,String);
	String p_ValueForKey3(String);
	c_Node4* p_LastNode();
	void mark();
};
class c_StringMap2 : public c_Map4{
	public:
	c_StringMap2();
	c_StringMap2* m_new();
	int p_Compare6(String,String);
	void mark();
};
class c_Node4 : public Object{
	public:
	String m_key;
	c_Node4* m_right;
	c_Node4* m_left;
	String m_value;
	int m_color;
	c_Node4* m_parent;
	c_Node4();
	c_Node4* m_new(String,String,int,c_Node4*);
	c_Node4* m_new2();
	int p_Count2(int);
	c_Node4* p_NextNode();
	String p_Key();
	String p_Value();
	c_Node4* p_PrevNode();
	c_Node4* p_Copy2(c_Node4*);
	void mark();
};
class c_MapKeys : public Object{
	public:
	c_Map4* m_map;
	c_MapKeys();
	c_MapKeys* m_new(c_Map4*);
	c_MapKeys* m_new2();
	c_KeyEnumerator* p_ObjectEnumerator();
	void mark();
};
class c_KeyEnumerator : public Object{
	public:
	c_Node4* m_node;
	c_KeyEnumerator();
	c_KeyEnumerator* m_new(c_Node4*);
	c_KeyEnumerator* m_new2();
	bool p_HasNext();
	String p_NextObject();
	void mark();
};
int bb_graphics_SetMatrix(Float,Float,Float,Float,Float,Float);
int bb_graphics_SetMatrix2(Array<Float >);
int bb_graphics_Transform(Float,Float,Float,Float,Float,Float);
int bb_graphics_Transform2(Array<Float >);
int bb_graphics_Translate(Float,Float);
void bb_functions_TranslateV(c_Vec2*);
int bb_graphics_Rotate(Float);
int bb_graphics_Scale(Float,Float);
void bb_functions_ScaleV(c_Vec2*);
class c_VShape : public c_VEntity{
	public:
	c_Color* m_color;
	bool m_renderOutline;
	c_VShape();
	virtual Float p_Radius()=0;
	virtual void p_Draw()=0;
	virtual void p_DrawOutline()=0;
	void p_Render();
	virtual bool p_PointInside(c_Vec2*)=0;
	virtual bool p_CollidesWith(c_VRect*)=0;
	virtual bool p_CollidesWith2(c_VCircle*)=0;
	bool p_CollidesWith3(c_VShape*);
	c_VShape* m_new();
	void mark();
};
int bb_graphics_PushMatrix();
int bb_graphics_PopMatrix();
class c_VRect : public c_VShape{
	public:
	c_Vec2* m_size;
	c_VRect();
	c_VRect* m_new(Float,Float,Float,Float);
	c_VRect* m_new2(c_Vec2*,c_Vec2*,c_Vec2*,c_Vec2*);
	c_VRect* m_new3();
	c_VRect* p_Copy();
	Float p_Radius();
	void p_Draw();
	void p_DrawOutline();
	c_Vec2* p_TopLeft();
	c_Vec2* p_TopRightUntransformed();
	c_Vec2* p_TopRight();
	c_Vec2* p_BottomLeftUntransformed();
	c_Vec2* p_BottomLeft();
	c_Vec2* p_BottomRightUntransformed();
	c_Vec2* p_BottomRight();
	c_Vec2* p_TopLeftUntransformed();
	bool p_PointInside(c_Vec2*);
	bool p_CollidesWith(c_VRect*);
	bool p_CollidesWith2(c_VCircle*);
	void mark();
};
class c_VCircle : public c_VShape{
	public:
	Float m_radius;
	c_VCircle();
	c_VCircle* m_new(Float,Float,Float);
	c_VCircle* m_new2();
	c_VCircle* p_Copy();
	bool p_CollidesWithLine(c_Vec2*,c_Vec2*);
	void p_Draw();
	void p_DrawOutline();
	Float p_Radius();
	bool p_PointInside(c_Vec2*);
	bool p_CollidesWith2(c_VCircle*);
	bool p_CollidesWith(c_VRect*);
	void mark();
};
Float bb_math2_RectRadius(Float,Float);
int bb_graphics_DrawRect(Float,Float,Float,Float);
int bb_graphics_DrawLine(Float,Float,Float,Float);
void bb_functions_DrawRectOutline(Float,Float,Float,Float);
c_Vec2* bb_math2_RotatePoint(c_Vec2*,Float,c_Vec2*);
bool bb_math2_PointInRect(Float,Float,Float,Float,Float,Float);
bool bb_math2_RectsOverlap(Float,Float,Float,Float,Float,Float,Float,Float);
bool bb_math2_LinesIntersect(c_Vec2*,c_Vec2*,c_Vec2*,c_Vec2*);
Float bb_math2_PerpendicularDistance(c_Vec2*,c_Vec2*,c_Vec2*);
int bb_graphics_DrawCircle(Float,Float,Float);
void bb_functions_DrawCircleOutline(Float,Float,Float,int);
Float bb_math2_DistanceOfPoints(Float,Float,Float,Float);
bool bb_math2_PointInCircle(Float,Float,Float,Float,Float);
bool bb_math2_CirclesOverlap(Float,Float,Float,Float,Float,Float);
class c_VSprite : public c_VEntity{
	public:
	c_Color* m_color;
	bool m_hidden;
	bool m_flipX;
	bool m_flipY;
	c_Image* m_image;
	String m_imagePath;
	c_VSprite();
	void p_SetImage(String,int);
	c_VSprite* m_new(String,Float,Float);
	c_VSprite* m_new2();
	c_VSprite* p_Copy();
	void p_SetHandle(Float,Float);
	void p_SetColor(Float,Float,Float);
	Float p_Width();
	Float p_Height();
	String p_ImagePath();
	Float p_HandleX();
	Float p_HandleY();
	void p_Alpha2(Float);
	Float p_Alpha();
	void p_DrawImage();
	void p_Render();
	void p_Update4(Float);
	void mark();
};
class c_Image : public Object{
	public:
	gxtkSurface* m_surface;
	int m_width;
	int m_height;
	Array<c_Frame* > m_frames;
	int m_flags;
	Float m_tx;
	Float m_ty;
	c_Image* m_source;
	c_Image();
	static int m_DefaultFlags;
	c_Image* m_new();
	int p_SetHandle(Float,Float);
	int p_ApplyFlags(int);
	c_Image* p_Init(gxtkSurface*,int,int);
	c_Image* p_Init2(gxtkSurface*,int,int,int,int,int,int,c_Image*,int,int,int,int);
	int p_Width();
	int p_Height();
	Float p_HandleX();
	Float p_HandleY();
	void mark();
};
class c_ImageCache : public Object{
	public:
	c_ImageCache();
	static c_StringMap3* m_ImageCache;
	static c_Image* m_GetImage(String,int);
	void mark();
};
class c_Map5 : public Object{
	public:
	c_Node5* m_root;
	c_Map5();
	c_Map5* m_new();
	virtual int p_Compare6(String,String)=0;
	c_Node5* p_FindNode3(String);
	bool p_Contains3(String);
	c_Image* p_Get3(String);
	int p_RotateLeft6(c_Node5*);
	int p_RotateRight5(c_Node5*);
	int p_InsertFixup5(c_Node5*);
	bool p_Set13(String,c_Image*);
	int p_Clear();
	int p_Count();
	bool p_IsEmpty();
	bool p_Add7(String,c_Image*);
	bool p_Update6(String,c_Image*);
	int p_DeleteFixup5(c_Node5*,c_Node5*);
	int p_RemoveNode5(c_Node5*);
	int p_Remove4(String);
	c_MapKeys5* p_Keys();
	c_MapValues5* p_Values();
	c_Node5* p_FirstNode();
	c_NodeEnumerator5* p_ObjectEnumerator();
	bool p_Insert11(String,c_Image*);
	c_Image* p_ValueForKey3(String);
	c_Node5* p_LastNode();
	void mark();
};
class c_StringMap3 : public c_Map5{
	public:
	c_StringMap3();
	c_StringMap3* m_new();
	int p_Compare6(String,String);
	void mark();
};
class c_Node5 : public Object{
	public:
	String m_key;
	c_Node5* m_right;
	c_Node5* m_left;
	c_Image* m_value;
	int m_color;
	c_Node5* m_parent;
	c_Node5();
	c_Node5* m_new(String,c_Image*,int,c_Node5*);
	c_Node5* m_new2();
	int p_Count2(int);
	String p_Key();
	c_Image* p_Value();
	c_Node5* p_NextNode();
	c_Node5* p_PrevNode();
	c_Node5* p_Copy3(c_Node5*);
	void mark();
};
String bb_data_FixDataPath(String);
extern gxtkGraphics* bb_graphics_device;
class c_Frame : public Object{
	public:
	int m_x;
	int m_y;
	c_Frame();
	c_Frame* m_new(int,int);
	c_Frame* m_new2();
	void mark();
};
c_Image* bb_graphics_LoadImage(String,int,int);
c_Image* bb_graphics_LoadImage2(String,int,int,int,int);
class c_Exception : public ThrowableObject{
	public:
	String m_message;
	c_Exception();
	c_Exception* m_new();
	c_Exception* m_new2(String);
	String p_ToString();
	void mark();
};
int bb_graphics_DrawImage(c_Image*,Float,Float,int);
int bb_graphics_DrawImage2(c_Image*,Float,Float,Float,Float,Float,int);
class c_Enumerator2 : public Object{
	public:
	c_Deque* m__deque;
	int m__index;
	c_Enumerator2();
	c_Enumerator2* m_new(c_Deque*);
	c_Enumerator2* m_new2();
	bool p_HasNext();
	int p_NextObject();
	void mark();
};
class c_Enumerator3 : public Object{
	public:
	c_Deque2* m__deque;
	int m__index;
	c_Enumerator3();
	c_Enumerator3* m_new(c_Deque2*);
	c_Enumerator3* m_new2();
	bool p_HasNext();
	Float p_NextObject();
	void mark();
};
class c_Enumerator4 : public Object{
	public:
	c_Deque3* m__deque;
	int m__index;
	c_Enumerator4();
	c_Enumerator4* m_new(c_Deque3*);
	c_Enumerator4* m_new2();
	bool p_HasNext();
	String p_NextObject();
	void mark();
};
class c_Enumerator5 : public Object{
	public:
	c_List* m__list;
	c_Node* m__curr;
	c_Enumerator5();
	c_Enumerator5* m_new(c_List*);
	c_Enumerator5* m_new2();
	bool p_HasNext();
	int p_NextObject();
	void mark();
};
class c_BackwardsList : public Object{
	public:
	c_List* m__list;
	c_BackwardsList();
	c_BackwardsList* m_new(c_List*);
	c_BackwardsList* m_new2();
	c_BackwardsEnumerator* p_ObjectEnumerator();
	void mark();
};
class c_Enumerator6 : public Object{
	public:
	c_List2* m__list;
	c_Node2* m__curr;
	c_Enumerator6();
	c_Enumerator6* m_new(c_List2*);
	c_Enumerator6* m_new2();
	bool p_HasNext();
	Float p_NextObject();
	void mark();
};
class c_BackwardsList2 : public Object{
	public:
	c_List2* m__list;
	c_BackwardsList2();
	c_BackwardsList2* m_new(c_List2*);
	c_BackwardsList2* m_new2();
	c_BackwardsEnumerator2* p_ObjectEnumerator();
	void mark();
};
class c_BackwardsList3 : public Object{
	public:
	c_List3* m__list;
	c_BackwardsList3();
	c_BackwardsList3* m_new(c_List3*);
	c_BackwardsList3* m_new2();
	c_BackwardsEnumerator3* p_ObjectEnumerator();
	void mark();
};
class c_BackwardsEnumerator : public Object{
	public:
	c_List* m__list;
	c_Node* m__curr;
	c_BackwardsEnumerator();
	c_BackwardsEnumerator* m_new(c_List*);
	c_BackwardsEnumerator* m_new2();
	bool p_HasNext();
	int p_NextObject();
	void mark();
};
class c_BackwardsEnumerator2 : public Object{
	public:
	c_List2* m__list;
	c_Node2* m__curr;
	c_BackwardsEnumerator2();
	c_BackwardsEnumerator2* m_new(c_List2*);
	c_BackwardsEnumerator2* m_new2();
	bool p_HasNext();
	Float p_NextObject();
	void mark();
};
class c_BackwardsEnumerator3 : public Object{
	public:
	c_List3* m__list;
	c_Node3* m__curr;
	c_BackwardsEnumerator3();
	c_BackwardsEnumerator3* m_new(c_List3*);
	c_BackwardsEnumerator3* m_new2();
	bool p_HasNext();
	String p_NextObject();
	void mark();
};
class c_Node6 : public Object{
	public:
	c_Node6* m_left;
	c_Node6* m_right;
	int m_key;
	Object* m_value;
	int m_color;
	c_Node6* m_parent;
	c_Node6();
	int p_Count2(int);
	c_Node6* m_new(int,Object*,int,c_Node6*);
	c_Node6* m_new2();
	int p_Key();
	Object* p_Value();
	c_Node6* p_NextNode();
	c_Node6* p_PrevNode();
	c_Node6* p_Copy4(c_Node6*);
	void mark();
};
class c_MapKeys2 : public Object{
	public:
	c_Map* m_map;
	c_MapKeys2();
	c_MapKeys2* m_new(c_Map*);
	c_MapKeys2* m_new2();
	c_KeyEnumerator2* p_ObjectEnumerator();
	void mark();
};
class c_MapValues : public Object{
	public:
	c_Map* m_map;
	c_MapValues();
	c_MapValues* m_new(c_Map*);
	c_MapValues* m_new2();
	c_ValueEnumerator* p_ObjectEnumerator();
	void mark();
};
class c_NodeEnumerator : public Object{
	public:
	c_Node6* m_node;
	c_NodeEnumerator();
	c_NodeEnumerator* m_new(c_Node6*);
	c_NodeEnumerator* m_new2();
	bool p_HasNext();
	c_Node6* p_NextObject();
	void mark();
};
class c_Node7 : public Object{
	public:
	c_Node7* m_left;
	c_Node7* m_right;
	Float m_key;
	Object* m_value;
	int m_color;
	c_Node7* m_parent;
	c_Node7();
	int p_Count2(int);
	c_Node7* m_new(Float,Object*,int,c_Node7*);
	c_Node7* m_new2();
	Float p_Key();
	Object* p_Value();
	c_Node7* p_NextNode();
	c_Node7* p_PrevNode();
	c_Node7* p_Copy5(c_Node7*);
	void mark();
};
class c_MapKeys3 : public Object{
	public:
	c_Map2* m_map;
	c_MapKeys3();
	c_MapKeys3* m_new(c_Map2*);
	c_MapKeys3* m_new2();
	c_KeyEnumerator3* p_ObjectEnumerator();
	void mark();
};
class c_MapValues2 : public Object{
	public:
	c_Map2* m_map;
	c_MapValues2();
	c_MapValues2* m_new(c_Map2*);
	c_MapValues2* m_new2();
	c_ValueEnumerator2* p_ObjectEnumerator();
	void mark();
};
class c_NodeEnumerator2 : public Object{
	public:
	c_Node7* m_node;
	c_NodeEnumerator2();
	c_NodeEnumerator2* m_new(c_Node7*);
	c_NodeEnumerator2* m_new2();
	bool p_HasNext();
	c_Node7* p_NextObject();
	void mark();
};
class c_Node8 : public Object{
	public:
	c_Node8* m_left;
	c_Node8* m_right;
	String m_key;
	Object* m_value;
	int m_color;
	c_Node8* m_parent;
	c_Node8();
	int p_Count2(int);
	c_Node8* m_new(String,Object*,int,c_Node8*);
	c_Node8* m_new2();
	String p_Key();
	Object* p_Value();
	c_Node8* p_NextNode();
	c_Node8* p_PrevNode();
	c_Node8* p_Copy6(c_Node8*);
	void mark();
};
class c_MapKeys4 : public Object{
	public:
	c_Map3* m_map;
	c_MapKeys4();
	c_MapKeys4* m_new(c_Map3*);
	c_MapKeys4* m_new2();
	c_KeyEnumerator4* p_ObjectEnumerator();
	void mark();
};
class c_MapValues3 : public Object{
	public:
	c_Map3* m_map;
	c_MapValues3();
	c_MapValues3* m_new(c_Map3*);
	c_MapValues3* m_new2();
	c_ValueEnumerator3* p_ObjectEnumerator();
	void mark();
};
class c_NodeEnumerator3 : public Object{
	public:
	c_Node8* m_node;
	c_NodeEnumerator3();
	c_NodeEnumerator3* m_new(c_Node8*);
	c_NodeEnumerator3* m_new2();
	bool p_HasNext();
	c_Node8* p_NextObject();
	void mark();
};
class c_MapValues4 : public Object{
	public:
	c_Map4* m_map;
	c_MapValues4();
	c_MapValues4* m_new(c_Map4*);
	c_MapValues4* m_new2();
	c_ValueEnumerator4* p_ObjectEnumerator();
	void mark();
};
class c_NodeEnumerator4 : public Object{
	public:
	c_Node4* m_node;
	c_NodeEnumerator4();
	c_NodeEnumerator4* m_new(c_Node4*);
	c_NodeEnumerator4* m_new2();
	bool p_HasNext();
	c_Node4* p_NextObject();
	void mark();
};
class c_MapKeys5 : public Object{
	public:
	c_Map5* m_map;
	c_MapKeys5();
	c_MapKeys5* m_new(c_Map5*);
	c_MapKeys5* m_new2();
	c_KeyEnumerator5* p_ObjectEnumerator();
	void mark();
};
class c_MapValues5 : public Object{
	public:
	c_Map5* m_map;
	c_MapValues5();
	c_MapValues5* m_new(c_Map5*);
	c_MapValues5* m_new2();
	c_ValueEnumerator5* p_ObjectEnumerator();
	void mark();
};
class c_NodeEnumerator5 : public Object{
	public:
	c_Node5* m_node;
	c_NodeEnumerator5();
	c_NodeEnumerator5* m_new(c_Node5*);
	c_NodeEnumerator5* m_new2();
	bool p_HasNext();
	c_Node5* p_NextObject();
	void mark();
};
class c_KeyEnumerator2 : public Object{
	public:
	c_Node6* m_node;
	c_KeyEnumerator2();
	c_KeyEnumerator2* m_new(c_Node6*);
	c_KeyEnumerator2* m_new2();
	bool p_HasNext();
	int p_NextObject();
	void mark();
};
class c_KeyEnumerator3 : public Object{
	public:
	c_Node7* m_node;
	c_KeyEnumerator3();
	c_KeyEnumerator3* m_new(c_Node7*);
	c_KeyEnumerator3* m_new2();
	bool p_HasNext();
	Float p_NextObject();
	void mark();
};
class c_KeyEnumerator4 : public Object{
	public:
	c_Node8* m_node;
	c_KeyEnumerator4();
	c_KeyEnumerator4* m_new(c_Node8*);
	c_KeyEnumerator4* m_new2();
	bool p_HasNext();
	String p_NextObject();
	void mark();
};
class c_KeyEnumerator5 : public Object{
	public:
	c_Node5* m_node;
	c_KeyEnumerator5();
	c_KeyEnumerator5* m_new(c_Node5*);
	c_KeyEnumerator5* m_new2();
	bool p_HasNext();
	String p_NextObject();
	void mark();
};
class c_ValueEnumerator : public Object{
	public:
	c_Node6* m_node;
	c_ValueEnumerator();
	c_ValueEnumerator* m_new(c_Node6*);
	c_ValueEnumerator* m_new2();
	bool p_HasNext();
	Object* p_NextObject();
	void mark();
};
class c_ValueEnumerator2 : public Object{
	public:
	c_Node7* m_node;
	c_ValueEnumerator2();
	c_ValueEnumerator2* m_new(c_Node7*);
	c_ValueEnumerator2* m_new2();
	bool p_HasNext();
	Object* p_NextObject();
	void mark();
};
class c_ValueEnumerator3 : public Object{
	public:
	c_Node8* m_node;
	c_ValueEnumerator3();
	c_ValueEnumerator3* m_new(c_Node8*);
	c_ValueEnumerator3* m_new2();
	bool p_HasNext();
	Object* p_NextObject();
	void mark();
};
class c_ValueEnumerator4 : public Object{
	public:
	c_Node4* m_node;
	c_ValueEnumerator4();
	c_ValueEnumerator4* m_new(c_Node4*);
	c_ValueEnumerator4* m_new2();
	bool p_HasNext();
	String p_NextObject();
	void mark();
};
class c_ValueEnumerator5 : public Object{
	public:
	c_Node5* m_node;
	c_ValueEnumerator5();
	c_ValueEnumerator5* m_new(c_Node5*);
	c_ValueEnumerator5* m_new2();
	bool p_HasNext();
	c_Image* p_NextObject();
	void mark();
};
class c_Enumerator7 : public Object{
	public:
	c_Stack* m_stack;
	int m_index;
	c_Enumerator7();
	c_Enumerator7* m_new(c_Stack*);
	c_Enumerator7* m_new2();
	bool p_HasNext();
	int p_NextObject();
	void mark();
};
class c_BackwardsStack : public Object{
	public:
	c_Stack* m_stack;
	c_BackwardsStack();
	c_BackwardsStack* m_new(c_Stack*);
	c_BackwardsStack* m_new2();
	c_BackwardsEnumerator4* p_ObjectEnumerator();
	void mark();
};
class c_Enumerator8 : public Object{
	public:
	c_Stack2* m_stack;
	int m_index;
	c_Enumerator8();
	c_Enumerator8* m_new(c_Stack2*);
	c_Enumerator8* m_new2();
	bool p_HasNext();
	Float p_NextObject();
	void mark();
};
class c_BackwardsStack2 : public Object{
	public:
	c_Stack2* m_stack;
	c_BackwardsStack2();
	c_BackwardsStack2* m_new(c_Stack2*);
	c_BackwardsStack2* m_new2();
	c_BackwardsEnumerator5* p_ObjectEnumerator();
	void mark();
};
class c_Enumerator9 : public Object{
	public:
	c_Stack3* m_stack;
	int m_index;
	c_Enumerator9();
	c_Enumerator9* m_new(c_Stack3*);
	c_Enumerator9* m_new2();
	bool p_HasNext();
	String p_NextObject();
	void mark();
};
class c_BackwardsStack3 : public Object{
	public:
	c_Stack3* m_stack;
	c_BackwardsStack3();
	c_BackwardsStack3* m_new(c_Stack3*);
	c_BackwardsStack3* m_new2();
	c_BackwardsEnumerator6* p_ObjectEnumerator();
	void mark();
};
class c_BackwardsEnumerator4 : public Object{
	public:
	c_Stack* m_stack;
	int m_index;
	c_BackwardsEnumerator4();
	c_BackwardsEnumerator4* m_new(c_Stack*);
	c_BackwardsEnumerator4* m_new2();
	bool p_HasNext();
	int p_NextObject();
	void mark();
};
class c_BackwardsEnumerator5 : public Object{
	public:
	c_Stack2* m_stack;
	int m_index;
	c_BackwardsEnumerator5();
	c_BackwardsEnumerator5* m_new(c_Stack2*);
	c_BackwardsEnumerator5* m_new2();
	bool p_HasNext();
	Float p_NextObject();
	void mark();
};
class c_BackwardsEnumerator6 : public Object{
	public:
	c_Stack3* m_stack;
	int m_index;
	c_BackwardsEnumerator6();
	c_BackwardsEnumerator6* m_new(c_Stack3*);
	c_BackwardsEnumerator6* m_new2();
	bool p_HasNext();
	String p_NextObject();
	void mark();
};
class c_ArrayObject : public Object{
	public:
	Array<int > m_value;
	c_ArrayObject();
	c_ArrayObject* m_new(Array<int >);
	Array<int > p_ToArray();
	c_ArrayObject* m_new2();
	void mark();
};
class c_ArrayObject2 : public Object{
	public:
	Array<Float > m_value;
	c_ArrayObject2();
	c_ArrayObject2* m_new(Array<Float >);
	Array<Float > p_ToArray();
	c_ArrayObject2* m_new2();
	void mark();
};
class c_ArrayObject3 : public Object{
	public:
	Array<String > m_value;
	c_ArrayObject3();
	c_ArrayObject3* m_new(Array<String >);
	Array<String > p_ToArray();
	c_ArrayObject3* m_new2();
	void mark();
};
class c_ClassInfo : public Object{
	public:
	String m__name;
	int m__attrs;
	c_ClassInfo* m__sclass;
	Array<c_ClassInfo* > m__ifaces;
	Array<c_ConstInfo* > m__rconsts;
	Array<c_ConstInfo* > m__consts;
	Array<c_FieldInfo* > m__rfields;
	Array<c_FieldInfo* > m__fields;
	Array<c_GlobalInfo* > m__rglobals;
	Array<c_GlobalInfo* > m__globals;
	Array<c_MethodInfo* > m__rmethods;
	Array<c_MethodInfo* > m__methods;
	Array<c_FunctionInfo* > m__rfunctions;
	Array<c_FunctionInfo* > m__functions;
	Array<c_FunctionInfo* > m__ctors;
	c_ClassInfo();
	c_ClassInfo* m_new(String,int,c_ClassInfo*,Array<c_ClassInfo* >);
	c_ClassInfo* m_new2();
	virtual int p_Init3();
	int p_InitR();
	void mark();
};
extern Array<c_ClassInfo* > bb_reflection__classes;
class c_R63 : public c_ClassInfo{
	public:
	c_R63();
	c_R63* m_new();
	int p_Init3();
	void mark();
};
class c_R64 : public c_ClassInfo{
	public:
	c_R64();
	c_R64* m_new();
	int p_Init3();
	void mark();
};
extern c_ClassInfo* bb_reflection__boolClass;
class c_R70 : public c_ClassInfo{
	public:
	c_R70();
	c_R70* m_new();
	int p_Init3();
	void mark();
};
extern c_ClassInfo* bb_reflection__intClass;
class c_R80 : public c_ClassInfo{
	public:
	c_R80();
	c_R80* m_new();
	int p_Init3();
	void mark();
};
extern c_ClassInfo* bb_reflection__floatClass;
class c_R90 : public c_ClassInfo{
	public:
	c_R90();
	c_R90* m_new();
	int p_Init3();
	void mark();
};
extern c_ClassInfo* bb_reflection__stringClass;
class c_R99 : public c_ClassInfo{
	public:
	c_R99();
	c_R99* m_new();
	int p_Init3();
	void mark();
};
class c_R121 : public c_ClassInfo{
	public:
	c_R121();
	c_R121* m_new();
	int p_Init3();
	void mark();
};
class c_R124 : public c_ClassInfo{
	public:
	c_R124();
	c_R124* m_new();
	int p_Init3();
	void mark();
};
class c_R146 : public c_ClassInfo{
	public:
	c_R146();
	c_R146* m_new();
	int p_Init3();
	void mark();
};
class c_R149 : public c_ClassInfo{
	public:
	c_R149();
	c_R149* m_new();
	int p_Init3();
	void mark();
};
class c_R171 : public c_ClassInfo{
	public:
	c_R171();
	c_R171* m_new();
	int p_Init3();
	void mark();
};
class c_R174 : public c_ClassInfo{
	public:
	c_R174();
	c_R174* m_new();
	int p_Init3();
	void mark();
};
class c_R175 : public c_ClassInfo{
	public:
	c_R175();
	c_R175* m_new();
	int p_Init3();
	void mark();
};
class c_R209 : public c_ClassInfo{
	public:
	c_R209();
	c_R209* m_new();
	int p_Init3();
	void mark();
};
class c_R214 : public c_ClassInfo{
	public:
	c_R214();
	c_R214* m_new();
	int p_Init3();
	void mark();
};
class c_R225 : public c_ClassInfo{
	public:
	c_R225();
	c_R225* m_new();
	int p_Init3();
	void mark();
};
class c_R228 : public c_ClassInfo{
	public:
	c_R228();
	c_R228* m_new();
	int p_Init3();
	void mark();
};
class c_R262 : public c_ClassInfo{
	public:
	c_R262();
	c_R262* m_new();
	int p_Init3();
	void mark();
};
class c_R267 : public c_ClassInfo{
	public:
	c_R267();
	c_R267* m_new();
	int p_Init3();
	void mark();
};
class c_R278 : public c_ClassInfo{
	public:
	c_R278();
	c_R278* m_new();
	int p_Init3();
	void mark();
};
class c_R281 : public c_ClassInfo{
	public:
	c_R281();
	c_R281* m_new();
	int p_Init3();
	void mark();
};
class c_R315 : public c_ClassInfo{
	public:
	c_R315();
	c_R315* m_new();
	int p_Init3();
	void mark();
};
class c_R321 : public c_ClassInfo{
	public:
	c_R321();
	c_R321* m_new();
	int p_Init3();
	void mark();
};
class c_R332 : public c_ClassInfo{
	public:
	c_R332();
	c_R332* m_new();
	int p_Init3();
	void mark();
};
class c_R335 : public c_ClassInfo{
	public:
	c_R335();
	c_R335* m_new();
	int p_Init3();
	void mark();
};
class c_R342 : public c_ClassInfo{
	public:
	c_R342();
	c_R342* m_new();
	int p_Init3();
	void mark();
};
class c_R353 : public c_ClassInfo{
	public:
	c_R353();
	c_R353* m_new();
	int p_Init3();
	void mark();
};
class c_R355 : public c_ClassInfo{
	public:
	c_R355();
	c_R355* m_new();
	int p_Init3();
	void mark();
};
class c_R381 : public c_ClassInfo{
	public:
	c_R381();
	c_R381* m_new();
	int p_Init3();
	void mark();
};
class c_R384 : public c_ClassInfo{
	public:
	c_R384();
	c_R384* m_new();
	int p_Init3();
	void mark();
};
class c_R395 : public c_ClassInfo{
	public:
	c_R395();
	c_R395* m_new();
	int p_Init3();
	void mark();
};
class c_R397 : public c_ClassInfo{
	public:
	c_R397();
	c_R397* m_new();
	int p_Init3();
	void mark();
};
class c_R423 : public c_ClassInfo{
	public:
	c_R423();
	c_R423* m_new();
	int p_Init3();
	void mark();
};
class c_R426 : public c_ClassInfo{
	public:
	c_R426();
	c_R426* m_new();
	int p_Init3();
	void mark();
};
class c_R437 : public c_ClassInfo{
	public:
	c_R437();
	c_R437* m_new();
	int p_Init3();
	void mark();
};
class c_R439 : public c_ClassInfo{
	public:
	c_R439();
	c_R439* m_new();
	int p_Init3();
	void mark();
};
class c_R465 : public c_ClassInfo{
	public:
	c_R465();
	c_R465* m_new();
	int p_Init3();
	void mark();
};
class c_R468 : public c_ClassInfo{
	public:
	c_R468();
	c_R468* m_new();
	int p_Init3();
	void mark();
};
class c_R505 : public c_ClassInfo{
	public:
	c_R505();
	c_R505* m_new();
	int p_Init3();
	void mark();
};
class c_R510 : public c_ClassInfo{
	public:
	c_R510();
	c_R510* m_new();
	int p_Init3();
	void mark();
};
class c_R547 : public c_ClassInfo{
	public:
	c_R547();
	c_R547* m_new();
	int p_Init3();
	void mark();
};
class c_R552 : public c_ClassInfo{
	public:
	c_R552();
	c_R552* m_new();
	int p_Init3();
	void mark();
};
class c_R589 : public c_ClassInfo{
	public:
	c_R589();
	c_R589* m_new();
	int p_Init3();
	void mark();
};
class c_R595 : public c_ClassInfo{
	public:
	c_R595();
	c_R595* m_new();
	int p_Init3();
	void mark();
};
class c_R601 : public c_ClassInfo{
	public:
	c_R601();
	c_R601* m_new();
	int p_Init3();
	void mark();
};
class c_R651 : public c_ClassInfo{
	public:
	c_R651();
	c_R651* m_new();
	int p_Init3();
	void mark();
};
class c_R665 : public c_ClassInfo{
	public:
	c_R665();
	c_R665* m_new();
	int p_Init3();
	void mark();
};
class c_R705 : public c_ClassInfo{
	public:
	c_R705();
	c_R705* m_new();
	int p_Init3();
	void mark();
};
class c_R722 : public c_ClassInfo{
	public:
	c_R722();
	c_R722* m_new();
	int p_Init3();
	void mark();
};
class c_R748 : public c_ClassInfo{
	public:
	c_R748();
	c_R748* m_new();
	int p_Init3();
	void mark();
};
class c_R751 : public c_ClassInfo{
	public:
	c_R751();
	c_R751* m_new();
	int p_Init3();
	void mark();
};
class c_R766 : public c_ClassInfo{
	public:
	c_R766();
	c_R766* m_new();
	int p_Init3();
	void mark();
};
class c_R771 : public c_ClassInfo{
	public:
	c_R771();
	c_R771* m_new();
	int p_Init3();
	void mark();
};
class c_R777 : public c_ClassInfo{
	public:
	c_R777();
	c_R777* m_new();
	int p_Init3();
	void mark();
};
class c_R789 : public c_ClassInfo{
	public:
	c_R789();
	c_R789* m_new();
	int p_Init3();
	void mark();
};
class c_R809 : public c_ClassInfo{
	public:
	c_R809();
	c_R809* m_new();
	int p_Init3();
	void mark();
};
class c_R821 : public c_ClassInfo{
	public:
	c_R821();
	c_R821* m_new();
	int p_Init3();
	void mark();
};
class c_R843 : public c_ClassInfo{
	public:
	c_R843();
	c_R843* m_new();
	int p_Init3();
	void mark();
};
class c_R850 : public c_ClassInfo{
	public:
	c_R850();
	c_R850* m_new();
	int p_Init3();
	void mark();
};
class c_R857 : public c_ClassInfo{
	public:
	c_R857();
	c_R857* m_new();
	int p_Init3();
	void mark();
};
class c_R864 : public c_ClassInfo{
	public:
	c_R864();
	c_R864* m_new();
	int p_Init3();
	void mark();
};
class c_R871 : public c_ClassInfo{
	public:
	c_R871();
	c_R871* m_new();
	int p_Init3();
	void mark();
};
class c_R876 : public c_ClassInfo{
	public:
	c_R876();
	c_R876* m_new();
	int p_Init3();
	void mark();
};
class c_R883 : public c_ClassInfo{
	public:
	c_R883();
	c_R883* m_new();
	int p_Init3();
	void mark();
};
class c_R888 : public c_ClassInfo{
	public:
	c_R888();
	c_R888* m_new();
	int p_Init3();
	void mark();
};
class c_R893 : public c_ClassInfo{
	public:
	c_R893();
	c_R893* m_new();
	int p_Init3();
	void mark();
};
class c_R900 : public c_ClassInfo{
	public:
	c_R900();
	c_R900* m_new();
	int p_Init3();
	void mark();
};
class c_R907 : public c_ClassInfo{
	public:
	c_R907();
	c_R907* m_new();
	int p_Init3();
	void mark();
};
class c_R914 : public c_ClassInfo{
	public:
	c_R914();
	c_R914* m_new();
	int p_Init3();
	void mark();
};
class c_R929 : public c_ClassInfo{
	public:
	c_R929();
	c_R929* m_new();
	int p_Init3();
	void mark();
};
class c_R934 : public c_ClassInfo{
	public:
	c_R934();
	c_R934* m_new();
	int p_Init3();
	void mark();
};
class c_R939 : public c_ClassInfo{
	public:
	c_R939();
	c_R939* m_new();
	int p_Init3();
	void mark();
};
class c_R945 : public c_ClassInfo{
	public:
	c_R945();
	c_R945* m_new();
	int p_Init3();
	void mark();
};
class c_R960 : public c_ClassInfo{
	public:
	c_R960();
	c_R960* m_new();
	int p_Init3();
	void mark();
};
class c_R965 : public c_ClassInfo{
	public:
	c_R965();
	c_R965* m_new();
	int p_Init3();
	void mark();
};
class c_R970 : public c_ClassInfo{
	public:
	c_R970();
	c_R970* m_new();
	int p_Init3();
	void mark();
};
class c_R976 : public c_ClassInfo{
	public:
	c_R976();
	c_R976* m_new();
	int p_Init3();
	void mark();
};
class c_R991 : public c_ClassInfo{
	public:
	c_R991();
	c_R991* m_new();
	int p_Init3();
	void mark();
};
class c_R996 : public c_ClassInfo{
	public:
	c_R996();
	c_R996* m_new();
	int p_Init3();
	void mark();
};
class c_R1001 : public c_ClassInfo{
	public:
	c_R1001();
	c_R1001* m_new();
	int p_Init3();
	void mark();
};
class c_R1007 : public c_ClassInfo{
	public:
	c_R1007();
	c_R1007* m_new();
	int p_Init3();
	void mark();
};
class c_R1012 : public c_ClassInfo{
	public:
	c_R1012();
	c_R1012* m_new();
	int p_Init3();
	void mark();
};
class c_R1018 : public c_ClassInfo{
	public:
	c_R1018();
	c_R1018* m_new();
	int p_Init3();
	void mark();
};
class c_R1024 : public c_ClassInfo{
	public:
	c_R1024();
	c_R1024* m_new();
	int p_Init3();
	void mark();
};
class c_R1030 : public c_ClassInfo{
	public:
	c_R1030();
	c_R1030* m_new();
	int p_Init3();
	void mark();
};
class c_R1036 : public c_ClassInfo{
	public:
	c_R1036();
	c_R1036* m_new();
	int p_Init3();
	void mark();
};
class c_R1042 : public c_ClassInfo{
	public:
	c_R1042();
	c_R1042* m_new();
	int p_Init3();
	void mark();
};
class c_R1048 : public c_ClassInfo{
	public:
	c_R1048();
	c_R1048* m_new();
	int p_Init3();
	void mark();
};
class c_R1054 : public c_ClassInfo{
	public:
	c_R1054();
	c_R1054* m_new();
	int p_Init3();
	void mark();
};
class c_R1060 : public c_ClassInfo{
	public:
	c_R1060();
	c_R1060* m_new();
	int p_Init3();
	void mark();
};
class c_R1067 : public c_ClassInfo{
	public:
	c_R1067();
	c_R1067* m_new();
	int p_Init3();
	void mark();
};
class c_R1072 : public c_ClassInfo{
	public:
	c_R1072();
	c_R1072* m_new();
	int p_Init3();
	void mark();
};
class c_R1079 : public c_ClassInfo{
	public:
	c_R1079();
	c_R1079* m_new();
	int p_Init3();
	void mark();
};
class c_R1084 : public c_ClassInfo{
	public:
	c_R1084();
	c_R1084* m_new();
	int p_Init3();
	void mark();
};
class c_R1091 : public c_ClassInfo{
	public:
	c_R1091();
	c_R1091* m_new();
	int p_Init3();
	void mark();
};
class c_R1096 : public c_ClassInfo{
	public:
	c_R1096();
	c_R1096* m_new();
	int p_Init3();
	void mark();
};
class c_R1103 : public c_ClassInfo{
	public:
	c_R1103();
	c_R1103* m_new();
	int p_Init3();
	void mark();
};
class c_R1110 : public c_ClassInfo{
	public:
	c_R1110();
	c_R1110* m_new();
	int p_Init3();
	void mark();
};
class c_R1117 : public c_ClassInfo{
	public:
	c_R1117();
	c_R1117* m_new();
	int p_Init3();
	void mark();
};
class c_R1122 : public c_ClassInfo{
	public:
	c_R1122();
	c_R1122* m_new();
	int p_Init3();
	void mark();
};
class c_R1127 : public c_ClassInfo{
	public:
	c_R1127();
	c_R1127* m_new();
	int p_Init3();
	void mark();
};
class c_ConstInfo : public Object{
	public:
	String m__name;
	int m__attrs;
	c_ClassInfo* m__type;
	Object* m__value;
	c_ConstInfo();
	c_ConstInfo* m_new(String,int,c_ClassInfo*,Object*);
	c_ConstInfo* m_new2();
	void mark();
};
extern Array<c_ConstInfo* > bb_reflection__consts;
class c_GlobalInfo : public Object{
	public:
	String m__name;
	int m__attrs;
	c_ClassInfo* m__type;
	c_GlobalInfo();
	c_GlobalInfo* m_new(String,int,c_ClassInfo*);
	c_GlobalInfo* m_new2();
	void mark();
};
extern Array<c_GlobalInfo* > bb_reflection__globals;
class c_R59 : public c_GlobalInfo{
	public:
	c_R59();
	c_R59* m_new();
	void mark();
};
class c_FunctionInfo : public Object{
	public:
	String m__name;
	int m__attrs;
	c_ClassInfo* m__retType;
	Array<c_ClassInfo* > m__argTypes;
	c_FunctionInfo();
	c_FunctionInfo* m_new(String,int,c_ClassInfo*,Array<c_ClassInfo* >);
	c_FunctionInfo* m_new2();
	void mark();
};
extern Array<c_FunctionInfo* > bb_reflection__functions;
class c_R17 : public c_FunctionInfo{
	public:
	c_R17();
	c_R17* m_new();
	void mark();
};
class c_R18 : public c_FunctionInfo{
	public:
	c_R18();
	c_R18* m_new();
	void mark();
};
class c_R19 : public c_FunctionInfo{
	public:
	c_R19();
	c_R19* m_new();
	void mark();
};
class c_R20 : public c_FunctionInfo{
	public:
	c_R20();
	c_R20* m_new();
	void mark();
};
class c_R21 : public c_FunctionInfo{
	public:
	c_R21();
	c_R21* m_new();
	void mark();
};
class c_R22 : public c_FunctionInfo{
	public:
	c_R22();
	c_R22* m_new();
	void mark();
};
class c_R23 : public c_FunctionInfo{
	public:
	c_R23();
	c_R23* m_new();
	void mark();
};
class c_R24 : public c_FunctionInfo{
	public:
	c_R24();
	c_R24* m_new();
	void mark();
};
class c_R25 : public c_FunctionInfo{
	public:
	c_R25();
	c_R25* m_new();
	void mark();
};
class c_R26 : public c_FunctionInfo{
	public:
	c_R26();
	c_R26* m_new();
	void mark();
};
class c_R27 : public c_FunctionInfo{
	public:
	c_R27();
	c_R27* m_new();
	void mark();
};
class c_R28 : public c_FunctionInfo{
	public:
	c_R28();
	c_R28* m_new();
	void mark();
};
class c_R29 : public c_FunctionInfo{
	public:
	c_R29();
	c_R29* m_new();
	void mark();
};
class c_R30 : public c_FunctionInfo{
	public:
	c_R30();
	c_R30* m_new();
	void mark();
};
class c_R31 : public c_FunctionInfo{
	public:
	c_R31();
	c_R31* m_new();
	void mark();
};
class c_R32 : public c_FunctionInfo{
	public:
	c_R32();
	c_R32* m_new();
	void mark();
};
class c_R33 : public c_FunctionInfo{
	public:
	c_R33();
	c_R33* m_new();
	void mark();
};
class c_R34 : public c_FunctionInfo{
	public:
	c_R34();
	c_R34* m_new();
	void mark();
};
class c_R35 : public c_FunctionInfo{
	public:
	c_R35();
	c_R35* m_new();
	void mark();
};
class c_R36 : public c_FunctionInfo{
	public:
	c_R36();
	c_R36* m_new();
	void mark();
};
class c_R37 : public c_FunctionInfo{
	public:
	c_R37();
	c_R37* m_new();
	void mark();
};
class c_R38 : public c_FunctionInfo{
	public:
	c_R38();
	c_R38* m_new();
	void mark();
};
class c_R39 : public c_FunctionInfo{
	public:
	c_R39();
	c_R39* m_new();
	void mark();
};
class c_R40 : public c_FunctionInfo{
	public:
	c_R40();
	c_R40* m_new();
	void mark();
};
class c_R41 : public c_FunctionInfo{
	public:
	c_R41();
	c_R41* m_new();
	void mark();
};
class c_R42 : public c_FunctionInfo{
	public:
	c_R42();
	c_R42* m_new();
	void mark();
};
class c_R43 : public c_FunctionInfo{
	public:
	c_R43();
	c_R43* m_new();
	void mark();
};
class c_R44 : public c_FunctionInfo{
	public:
	c_R44();
	c_R44* m_new();
	void mark();
};
class c_R45 : public c_FunctionInfo{
	public:
	c_R45();
	c_R45* m_new();
	void mark();
};
class c_R46 : public c_FunctionInfo{
	public:
	c_R46();
	c_R46* m_new();
	void mark();
};
class c_R47 : public c_FunctionInfo{
	public:
	c_R47();
	c_R47* m_new();
	void mark();
};
class c_R48 : public c_FunctionInfo{
	public:
	c_R48();
	c_R48* m_new();
	void mark();
};
class c_R49 : public c_FunctionInfo{
	public:
	c_R49();
	c_R49* m_new();
	void mark();
};
class c_R50 : public c_FunctionInfo{
	public:
	c_R50();
	c_R50* m_new();
	void mark();
};
class c_R51 : public c_FunctionInfo{
	public:
	c_R51();
	c_R51* m_new();
	void mark();
};
class c_R52 : public c_FunctionInfo{
	public:
	c_R52();
	c_R52* m_new();
	void mark();
};
class c_R53 : public c_FunctionInfo{
	public:
	c_R53();
	c_R53* m_new();
	void mark();
};
class c_R54 : public c_FunctionInfo{
	public:
	c_R54();
	c_R54* m_new();
	void mark();
};
class c_R55 : public c_FunctionInfo{
	public:
	c_R55();
	c_R55* m_new();
	void mark();
};
class c_R56 : public c_FunctionInfo{
	public:
	c_R56();
	c_R56* m_new();
	void mark();
};
class c_R57 : public c_FunctionInfo{
	public:
	c_R57();
	c_R57* m_new();
	void mark();
};
class c_R58 : public c_FunctionInfo{
	public:
	c_R58();
	c_R58* m_new();
	void mark();
};
class c_R60 : public c_FunctionInfo{
	public:
	c_R60();
	c_R60* m_new();
	void mark();
};
class c_R61 : public c_FunctionInfo{
	public:
	c_R61();
	c_R61* m_new();
	void mark();
};
class c_R62 : public c_FunctionInfo{
	public:
	c_R62();
	c_R62* m_new();
	void mark();
};
class c__GetClass : public Object{
	public:
	c__GetClass();
	c__GetClass* m_new();
	void mark();
};
class c___GetClass : public c__GetClass{
	public:
	c___GetClass();
	c___GetClass* m_new();
	void mark();
};
extern c__GetClass* bb_reflection__getClass;
int bb_reflection___init();
extern int bb_reflection__init;
class c_App : public Object{
	public:
	c_App();
	c_App* m_new();
	virtual int p_OnResize();
	virtual int p_OnCreate();
	virtual int p_OnSuspend();
	virtual int p_OnResume();
	virtual int p_OnUpdate();
	virtual int p_OnLoading();
	virtual int p_OnRender();
	int p_OnClose();
	int p_OnBack();
	void mark();
};
class c_VsatApp : public c_App{
	public:
	bool m_displayFps;
	c_VScene* m_activeScene;
	c_VTransition* m_transition;
	int m_screenWidth;
	int m_screenHeight;
	int m_screenWidth2;
	int m_screenHeight2;
	c_AngelFont* m_systemFont;
	bool m_paused;
	Float m_lastUpdate;
	Float m_deltaTime;
	Float m_seconds;
	c_VScene* m_nextScene;
	c_VsatApp();
	c_VsatApp* m_new();
	void p_ChangeScene(c_VScene*);
	void p_StartFadeIn(c_VTransition*);
	int p_OnLoading();
	void p_UpdateScreenSize();
	int p_OnCreate();
	void p_UpdateGameTime();
	int p_OnUpdate();
	void p_RenderFps();
	int p_OnRender();
	int p_OnSuspend();
	int p_OnResume();
	int p_OnResize();
	int p_ScreenWidth();
	int p_ScreenHeight();
	void mark();
};
extern c_App* bb_app__app;
class c_GameDelegate : public BBGameDelegate{
	public:
	gxtkGraphics* m__graphics;
	gxtkAudio* m__audio;
	c_InputDevice* m__input;
	c_GameDelegate();
	c_GameDelegate* m_new();
	void StartGame();
	void SuspendGame();
	void ResumeGame();
	void UpdateGame();
	void RenderGame();
	void KeyEvent(int,int);
	void MouseEvent(int,int,Float,Float);
	void TouchEvent(int,int,Float,Float);
	void MotionEvent(int,int,Float,Float,Float);
	void DiscardGraphics();
	void mark();
};
extern c_GameDelegate* bb_app__delegate;
extern BBGame* bb_app__game;
extern c_VsatApp* bb_app2_Vsat;
class c_VScene : public Object{
	public:
	c_VScene();
	c_VScene* m_new();
	void p_OnExit();
	virtual void p_OnInit();
	void p_OnLoading();
	void p_OnUpdateWhilePaused();
	virtual void p_OnUpdate2(Float);
	virtual void p_OnRender();
	void p_OnSuspend();
	void p_OnResume();
	void p_OnResize();
	void mark();
};
class c_VActionEventHandler : public virtual gc_interface{
	public:
	virtual void p_OnActionEvent(int,c_VAction*)=0;
};
class c_vvv : public c_VScene,public virtual c_VActionEventHandler{
	public:
	c_List4* m_shapes;
	int m_x;
	c_List6* m_actions;
	c_vvv();
	c_vvv* m_new();
	void p_OnInit();
	void p_OnUpdate2(Float);
	void p_OnRender();
	void p_OnActionEvent(int,c_VAction*);
	void mark();
};
void bb_functions2_Assert(Object*);
void bb_functions2_Assert2(bool);
class c_DummyScene : public c_VScene{
	public:
	c_VScene* m_initScene;
	c_DummyScene();
	c_DummyScene* m_new();
	void p_InitWithScene(c_VScene*);
	void p_OnUpdate2(Float);
	void mark();
};
int bbMain();
class c_Stack4 : public Object{
	public:
	Array<c_ConstInfo* > m_data;
	int m_length;
	c_Stack4();
	c_Stack4* m_new();
	c_Stack4* m_new2(Array<c_ConstInfo* >);
	void p_Push10(c_ConstInfo*);
	void p_Push11(Array<c_ConstInfo* >,int,int);
	void p_Push12(Array<c_ConstInfo* >,int);
	Array<c_ConstInfo* > p_ToArray();
	void mark();
};
class c_FieldInfo : public Object{
	public:
	String m__name;
	int m__attrs;
	c_ClassInfo* m__type;
	c_FieldInfo();
	c_FieldInfo* m_new(String,int,c_ClassInfo*);
	c_FieldInfo* m_new2();
	void mark();
};
class c_Stack5 : public Object{
	public:
	Array<c_FieldInfo* > m_data;
	int m_length;
	c_Stack5();
	c_Stack5* m_new();
	c_Stack5* m_new2(Array<c_FieldInfo* >);
	void p_Push13(c_FieldInfo*);
	void p_Push14(Array<c_FieldInfo* >,int,int);
	void p_Push15(Array<c_FieldInfo* >,int);
	Array<c_FieldInfo* > p_ToArray();
	void mark();
};
class c_Stack6 : public Object{
	public:
	Array<c_GlobalInfo* > m_data;
	int m_length;
	c_Stack6();
	c_Stack6* m_new();
	c_Stack6* m_new2(Array<c_GlobalInfo* >);
	void p_Push16(c_GlobalInfo*);
	void p_Push17(Array<c_GlobalInfo* >,int,int);
	void p_Push18(Array<c_GlobalInfo* >,int);
	Array<c_GlobalInfo* > p_ToArray();
	void mark();
};
class c_MethodInfo : public Object{
	public:
	String m__name;
	int m__attrs;
	c_ClassInfo* m__retType;
	Array<c_ClassInfo* > m__argTypes;
	c_MethodInfo();
	c_MethodInfo* m_new(String,int,c_ClassInfo*,Array<c_ClassInfo* >);
	c_MethodInfo* m_new2();
	void mark();
};
class c_Stack7 : public Object{
	public:
	Array<c_MethodInfo* > m_data;
	int m_length;
	c_Stack7();
	c_Stack7* m_new();
	c_Stack7* m_new2(Array<c_MethodInfo* >);
	void p_Push19(c_MethodInfo*);
	void p_Push20(Array<c_MethodInfo* >,int,int);
	void p_Push21(Array<c_MethodInfo* >,int);
	Array<c_MethodInfo* > p_ToArray();
	void mark();
};
class c_Stack8 : public Object{
	public:
	Array<c_FunctionInfo* > m_data;
	int m_length;
	c_Stack8();
	c_Stack8* m_new();
	c_Stack8* m_new2(Array<c_FunctionInfo* >);
	void p_Push22(c_FunctionInfo*);
	void p_Push23(Array<c_FunctionInfo* >,int,int);
	void p_Push24(Array<c_FunctionInfo* >,int);
	Array<c_FunctionInfo* > p_ToArray();
	void mark();
};
class c_R65 : public c_FieldInfo{
	public:
	c_R65();
	c_R65* m_new();
	void mark();
};
class c_R67 : public c_MethodInfo{
	public:
	c_R67();
	c_R67* m_new();
	void mark();
};
class c_R68 : public c_MethodInfo{
	public:
	c_R68();
	c_R68* m_new();
	void mark();
};
class c_R66 : public c_FunctionInfo{
	public:
	c_R66();
	c_R66* m_new();
	void mark();
};
class c_R69 : public c_FunctionInfo{
	public:
	c_R69();
	c_R69* m_new();
	void mark();
};
class c_R71 : public c_FieldInfo{
	public:
	c_R71();
	c_R71* m_new();
	void mark();
};
class c_R74 : public c_MethodInfo{
	public:
	c_R74();
	c_R74* m_new();
	void mark();
};
class c_R75 : public c_MethodInfo{
	public:
	c_R75();
	c_R75* m_new();
	void mark();
};
class c_R76 : public c_MethodInfo{
	public:
	c_R76();
	c_R76* m_new();
	void mark();
};
class c_R77 : public c_MethodInfo{
	public:
	c_R77();
	c_R77* m_new();
	void mark();
};
class c_R78 : public c_MethodInfo{
	public:
	c_R78();
	c_R78* m_new();
	void mark();
};
class c_R72 : public c_FunctionInfo{
	public:
	c_R72();
	c_R72* m_new();
	void mark();
};
class c_R73 : public c_FunctionInfo{
	public:
	c_R73();
	c_R73* m_new();
	void mark();
};
class c_R79 : public c_FunctionInfo{
	public:
	c_R79();
	c_R79* m_new();
	void mark();
};
class c_R81 : public c_FieldInfo{
	public:
	c_R81();
	c_R81* m_new();
	void mark();
};
class c_R84 : public c_MethodInfo{
	public:
	c_R84();
	c_R84* m_new();
	void mark();
};
class c_R85 : public c_MethodInfo{
	public:
	c_R85();
	c_R85* m_new();
	void mark();
};
class c_R86 : public c_MethodInfo{
	public:
	c_R86();
	c_R86* m_new();
	void mark();
};
class c_R87 : public c_MethodInfo{
	public:
	c_R87();
	c_R87* m_new();
	void mark();
};
class c_R88 : public c_MethodInfo{
	public:
	c_R88();
	c_R88* m_new();
	void mark();
};
class c_R82 : public c_FunctionInfo{
	public:
	c_R82();
	c_R82* m_new();
	void mark();
};
class c_R83 : public c_FunctionInfo{
	public:
	c_R83();
	c_R83* m_new();
	void mark();
};
class c_R89 : public c_FunctionInfo{
	public:
	c_R89();
	c_R89* m_new();
	void mark();
};
class c_R91 : public c_FieldInfo{
	public:
	c_R91();
	c_R91* m_new();
	void mark();
};
class c_R95 : public c_MethodInfo{
	public:
	c_R95();
	c_R95* m_new();
	void mark();
};
class c_R96 : public c_MethodInfo{
	public:
	c_R96();
	c_R96* m_new();
	void mark();
};
class c_R97 : public c_MethodInfo{
	public:
	c_R97();
	c_R97* m_new();
	void mark();
};
class c_R92 : public c_FunctionInfo{
	public:
	c_R92();
	c_R92* m_new();
	void mark();
};
class c_R93 : public c_FunctionInfo{
	public:
	c_R93();
	c_R93* m_new();
	void mark();
};
class c_R94 : public c_FunctionInfo{
	public:
	c_R94();
	c_R94* m_new();
	void mark();
};
class c_R98 : public c_FunctionInfo{
	public:
	c_R98();
	c_R98* m_new();
	void mark();
};
class c_R115 : public c_GlobalInfo{
	public:
	c_R115();
	c_R115* m_new();
	void mark();
};
class c_R116 : public c_FieldInfo{
	public:
	c_R116();
	c_R116* m_new();
	void mark();
};
class c_R117 : public c_FieldInfo{
	public:
	c_R117();
	c_R117* m_new();
	void mark();
};
class c_R118 : public c_FieldInfo{
	public:
	c_R118();
	c_R118* m_new();
	void mark();
};
class c_R119 : public c_FieldInfo{
	public:
	c_R119();
	c_R119* m_new();
	void mark();
};
class c_R102 : public c_MethodInfo{
	public:
	c_R102();
	c_R102* m_new();
	void mark();
};
class c_R103 : public c_MethodInfo{
	public:
	c_R103();
	c_R103* m_new();
	void mark();
};
class c_R104 : public c_MethodInfo{
	public:
	c_R104();
	c_R104* m_new();
	void mark();
};
class c_R105 : public c_MethodInfo{
	public:
	c_R105();
	c_R105* m_new();
	void mark();
};
class c_R106 : public c_MethodInfo{
	public:
	c_R106();
	c_R106* m_new();
	void mark();
};
class c_R107 : public c_MethodInfo{
	public:
	c_R107();
	c_R107* m_new();
	void mark();
};
class c_R108 : public c_MethodInfo{
	public:
	c_R108();
	c_R108* m_new();
	void mark();
};
class c_R109 : public c_MethodInfo{
	public:
	c_R109();
	c_R109* m_new();
	void mark();
};
class c_R110 : public c_MethodInfo{
	public:
	c_R110();
	c_R110* m_new();
	void mark();
};
class c_R111 : public c_MethodInfo{
	public:
	c_R111();
	c_R111* m_new();
	void mark();
};
class c_R112 : public c_MethodInfo{
	public:
	c_R112();
	c_R112* m_new();
	void mark();
};
class c_R113 : public c_MethodInfo{
	public:
	c_R113();
	c_R113* m_new();
	void mark();
};
class c_R114 : public c_MethodInfo{
	public:
	c_R114();
	c_R114* m_new();
	void mark();
};
class c_R120 : public c_MethodInfo{
	public:
	c_R120();
	c_R120* m_new();
	void mark();
};
class c_R100 : public c_FunctionInfo{
	public:
	c_R100();
	c_R100* m_new();
	void mark();
};
class c_R101 : public c_FunctionInfo{
	public:
	c_R101();
	c_R101* m_new();
	void mark();
};
class c_R122 : public c_FunctionInfo{
	public:
	c_R122();
	c_R122* m_new();
	void mark();
};
class c_R123 : public c_FunctionInfo{
	public:
	c_R123();
	c_R123* m_new();
	void mark();
};
class c_R140 : public c_GlobalInfo{
	public:
	c_R140();
	c_R140* m_new();
	void mark();
};
class c_R141 : public c_FieldInfo{
	public:
	c_R141();
	c_R141* m_new();
	void mark();
};
class c_R142 : public c_FieldInfo{
	public:
	c_R142();
	c_R142* m_new();
	void mark();
};
class c_R143 : public c_FieldInfo{
	public:
	c_R143();
	c_R143* m_new();
	void mark();
};
class c_R144 : public c_FieldInfo{
	public:
	c_R144();
	c_R144* m_new();
	void mark();
};
class c_R127 : public c_MethodInfo{
	public:
	c_R127();
	c_R127* m_new();
	void mark();
};
class c_R128 : public c_MethodInfo{
	public:
	c_R128();
	c_R128* m_new();
	void mark();
};
class c_R129 : public c_MethodInfo{
	public:
	c_R129();
	c_R129* m_new();
	void mark();
};
class c_R130 : public c_MethodInfo{
	public:
	c_R130();
	c_R130* m_new();
	void mark();
};
class c_R131 : public c_MethodInfo{
	public:
	c_R131();
	c_R131* m_new();
	void mark();
};
class c_R132 : public c_MethodInfo{
	public:
	c_R132();
	c_R132* m_new();
	void mark();
};
class c_R133 : public c_MethodInfo{
	public:
	c_R133();
	c_R133* m_new();
	void mark();
};
class c_R134 : public c_MethodInfo{
	public:
	c_R134();
	c_R134* m_new();
	void mark();
};
class c_R135 : public c_MethodInfo{
	public:
	c_R135();
	c_R135* m_new();
	void mark();
};
class c_R136 : public c_MethodInfo{
	public:
	c_R136();
	c_R136* m_new();
	void mark();
};
class c_R137 : public c_MethodInfo{
	public:
	c_R137();
	c_R137* m_new();
	void mark();
};
class c_R138 : public c_MethodInfo{
	public:
	c_R138();
	c_R138* m_new();
	void mark();
};
class c_R139 : public c_MethodInfo{
	public:
	c_R139();
	c_R139* m_new();
	void mark();
};
class c_R145 : public c_MethodInfo{
	public:
	c_R145();
	c_R145* m_new();
	void mark();
};
class c_R125 : public c_FunctionInfo{
	public:
	c_R125();
	c_R125* m_new();
	void mark();
};
class c_R126 : public c_FunctionInfo{
	public:
	c_R126();
	c_R126* m_new();
	void mark();
};
class c_R147 : public c_FunctionInfo{
	public:
	c_R147();
	c_R147* m_new();
	void mark();
};
class c_R148 : public c_FunctionInfo{
	public:
	c_R148();
	c_R148* m_new();
	void mark();
};
class c_R165 : public c_GlobalInfo{
	public:
	c_R165();
	c_R165* m_new();
	void mark();
};
class c_R166 : public c_FieldInfo{
	public:
	c_R166();
	c_R166* m_new();
	void mark();
};
class c_R167 : public c_FieldInfo{
	public:
	c_R167();
	c_R167* m_new();
	void mark();
};
class c_R168 : public c_FieldInfo{
	public:
	c_R168();
	c_R168* m_new();
	void mark();
};
class c_R169 : public c_FieldInfo{
	public:
	c_R169();
	c_R169* m_new();
	void mark();
};
class c_R152 : public c_MethodInfo{
	public:
	c_R152();
	c_R152* m_new();
	void mark();
};
class c_R153 : public c_MethodInfo{
	public:
	c_R153();
	c_R153* m_new();
	void mark();
};
class c_R154 : public c_MethodInfo{
	public:
	c_R154();
	c_R154* m_new();
	void mark();
};
class c_R155 : public c_MethodInfo{
	public:
	c_R155();
	c_R155* m_new();
	void mark();
};
class c_R156 : public c_MethodInfo{
	public:
	c_R156();
	c_R156* m_new();
	void mark();
};
class c_R157 : public c_MethodInfo{
	public:
	c_R157();
	c_R157* m_new();
	void mark();
};
class c_R158 : public c_MethodInfo{
	public:
	c_R158();
	c_R158* m_new();
	void mark();
};
class c_R159 : public c_MethodInfo{
	public:
	c_R159();
	c_R159* m_new();
	void mark();
};
class c_R160 : public c_MethodInfo{
	public:
	c_R160();
	c_R160* m_new();
	void mark();
};
class c_R161 : public c_MethodInfo{
	public:
	c_R161();
	c_R161* m_new();
	void mark();
};
class c_R162 : public c_MethodInfo{
	public:
	c_R162();
	c_R162* m_new();
	void mark();
};
class c_R163 : public c_MethodInfo{
	public:
	c_R163();
	c_R163* m_new();
	void mark();
};
class c_R164 : public c_MethodInfo{
	public:
	c_R164();
	c_R164* m_new();
	void mark();
};
class c_R170 : public c_MethodInfo{
	public:
	c_R170();
	c_R170* m_new();
	void mark();
};
class c_R150 : public c_FunctionInfo{
	public:
	c_R150();
	c_R150* m_new();
	void mark();
};
class c_R151 : public c_FunctionInfo{
	public:
	c_R151();
	c_R151* m_new();
	void mark();
};
class c_R172 : public c_FunctionInfo{
	public:
	c_R172();
	c_R172* m_new();
	void mark();
};
class c_R173 : public c_FunctionInfo{
	public:
	c_R173();
	c_R173* m_new();
	void mark();
};
class c_R208 : public c_FieldInfo{
	public:
	c_R208();
	c_R208* m_new();
	void mark();
};
class c_R178 : public c_MethodInfo{
	public:
	c_R178();
	c_R178* m_new();
	void mark();
};
class c_R179 : public c_MethodInfo{
	public:
	c_R179();
	c_R179* m_new();
	void mark();
};
class c_R180 : public c_MethodInfo{
	public:
	c_R180();
	c_R180* m_new();
	void mark();
};
class c_R181 : public c_MethodInfo{
	public:
	c_R181();
	c_R181* m_new();
	void mark();
};
class c_R182 : public c_MethodInfo{
	public:
	c_R182();
	c_R182* m_new();
	void mark();
};
class c_R183 : public c_MethodInfo{
	public:
	c_R183();
	c_R183* m_new();
	void mark();
};
class c_R184 : public c_MethodInfo{
	public:
	c_R184();
	c_R184* m_new();
	void mark();
};
class c_R185 : public c_MethodInfo{
	public:
	c_R185();
	c_R185* m_new();
	void mark();
};
class c_R186 : public c_MethodInfo{
	public:
	c_R186();
	c_R186* m_new();
	void mark();
};
class c_R187 : public c_MethodInfo{
	public:
	c_R187();
	c_R187* m_new();
	void mark();
};
class c_R188 : public c_MethodInfo{
	public:
	c_R188();
	c_R188* m_new();
	void mark();
};
class c_R189 : public c_MethodInfo{
	public:
	c_R189();
	c_R189* m_new();
	void mark();
};
class c_R190 : public c_MethodInfo{
	public:
	c_R190();
	c_R190* m_new();
	void mark();
};
class c_R191 : public c_MethodInfo{
	public:
	c_R191();
	c_R191* m_new();
	void mark();
};
class c_R192 : public c_MethodInfo{
	public:
	c_R192();
	c_R192* m_new();
	void mark();
};
class c_R193 : public c_MethodInfo{
	public:
	c_R193();
	c_R193* m_new();
	void mark();
};
class c_R194 : public c_MethodInfo{
	public:
	c_R194();
	c_R194* m_new();
	void mark();
};
class c_R195 : public c_MethodInfo{
	public:
	c_R195();
	c_R195* m_new();
	void mark();
};
class c_R196 : public c_MethodInfo{
	public:
	c_R196();
	c_R196* m_new();
	void mark();
};
class c_R197 : public c_MethodInfo{
	public:
	c_R197();
	c_R197* m_new();
	void mark();
};
class c_R198 : public c_MethodInfo{
	public:
	c_R198();
	c_R198* m_new();
	void mark();
};
class c_R199 : public c_MethodInfo{
	public:
	c_R199();
	c_R199* m_new();
	void mark();
};
class c_R200 : public c_MethodInfo{
	public:
	c_R200();
	c_R200* m_new();
	void mark();
};
class c_R201 : public c_MethodInfo{
	public:
	c_R201();
	c_R201* m_new();
	void mark();
};
class c_R202 : public c_MethodInfo{
	public:
	c_R202();
	c_R202* m_new();
	void mark();
};
class c_R203 : public c_MethodInfo{
	public:
	c_R203();
	c_R203* m_new();
	void mark();
};
class c_R204 : public c_MethodInfo{
	public:
	c_R204();
	c_R204* m_new();
	void mark();
};
class c_R205 : public c_MethodInfo{
	public:
	c_R205();
	c_R205* m_new();
	void mark();
};
class c_R206 : public c_MethodInfo{
	public:
	c_R206();
	c_R206* m_new();
	void mark();
};
class c_R207 : public c_MethodInfo{
	public:
	c_R207();
	c_R207* m_new();
	void mark();
};
class c_R176 : public c_FunctionInfo{
	public:
	c_R176();
	c_R176* m_new();
	void mark();
};
class c_R177 : public c_FunctionInfo{
	public:
	c_R177();
	c_R177* m_new();
	void mark();
};
class c_R211 : public c_MethodInfo{
	public:
	c_R211();
	c_R211* m_new();
	void mark();
};
class c_R212 : public c_MethodInfo{
	public:
	c_R212();
	c_R212* m_new();
	void mark();
};
class c_R210 : public c_FunctionInfo{
	public:
	c_R210();
	c_R210* m_new();
	void mark();
};
class c_R213 : public c_FunctionInfo{
	public:
	c_R213();
	c_R213* m_new();
	void mark();
};
class c_R220 : public c_FieldInfo{
	public:
	c_R220();
	c_R220* m_new();
	void mark();
};
class c_R221 : public c_FieldInfo{
	public:
	c_R221();
	c_R221* m_new();
	void mark();
};
class c_R222 : public c_FieldInfo{
	public:
	c_R222();
	c_R222* m_new();
	void mark();
};
class c_R216 : public c_MethodInfo{
	public:
	c_R216();
	c_R216* m_new();
	void mark();
};
class c_R217 : public c_MethodInfo{
	public:
	c_R217();
	c_R217* m_new();
	void mark();
};
class c_R218 : public c_MethodInfo{
	public:
	c_R218();
	c_R218* m_new();
	void mark();
};
class c_R219 : public c_MethodInfo{
	public:
	c_R219();
	c_R219* m_new();
	void mark();
};
class c_R223 : public c_MethodInfo{
	public:
	c_R223();
	c_R223* m_new();
	void mark();
};
class c_R215 : public c_FunctionInfo{
	public:
	c_R215();
	c_R215* m_new();
	void mark();
};
class c_R224 : public c_FunctionInfo{
	public:
	c_R224();
	c_R224* m_new();
	void mark();
};
class c_R227 : public c_MethodInfo{
	public:
	c_R227();
	c_R227* m_new();
	void mark();
};
class c_R226 : public c_FunctionInfo{
	public:
	c_R226();
	c_R226* m_new();
	void mark();
};
class c_R261 : public c_FieldInfo{
	public:
	c_R261();
	c_R261* m_new();
	void mark();
};
class c_R231 : public c_MethodInfo{
	public:
	c_R231();
	c_R231* m_new();
	void mark();
};
class c_R232 : public c_MethodInfo{
	public:
	c_R232();
	c_R232* m_new();
	void mark();
};
class c_R233 : public c_MethodInfo{
	public:
	c_R233();
	c_R233* m_new();
	void mark();
};
class c_R234 : public c_MethodInfo{
	public:
	c_R234();
	c_R234* m_new();
	void mark();
};
class c_R235 : public c_MethodInfo{
	public:
	c_R235();
	c_R235* m_new();
	void mark();
};
class c_R236 : public c_MethodInfo{
	public:
	c_R236();
	c_R236* m_new();
	void mark();
};
class c_R237 : public c_MethodInfo{
	public:
	c_R237();
	c_R237* m_new();
	void mark();
};
class c_R238 : public c_MethodInfo{
	public:
	c_R238();
	c_R238* m_new();
	void mark();
};
class c_R239 : public c_MethodInfo{
	public:
	c_R239();
	c_R239* m_new();
	void mark();
};
class c_R240 : public c_MethodInfo{
	public:
	c_R240();
	c_R240* m_new();
	void mark();
};
class c_R241 : public c_MethodInfo{
	public:
	c_R241();
	c_R241* m_new();
	void mark();
};
class c_R242 : public c_MethodInfo{
	public:
	c_R242();
	c_R242* m_new();
	void mark();
};
class c_R243 : public c_MethodInfo{
	public:
	c_R243();
	c_R243* m_new();
	void mark();
};
class c_R244 : public c_MethodInfo{
	public:
	c_R244();
	c_R244* m_new();
	void mark();
};
class c_R245 : public c_MethodInfo{
	public:
	c_R245();
	c_R245* m_new();
	void mark();
};
class c_R246 : public c_MethodInfo{
	public:
	c_R246();
	c_R246* m_new();
	void mark();
};
class c_R247 : public c_MethodInfo{
	public:
	c_R247();
	c_R247* m_new();
	void mark();
};
class c_R248 : public c_MethodInfo{
	public:
	c_R248();
	c_R248* m_new();
	void mark();
};
class c_R249 : public c_MethodInfo{
	public:
	c_R249();
	c_R249* m_new();
	void mark();
};
class c_R250 : public c_MethodInfo{
	public:
	c_R250();
	c_R250* m_new();
	void mark();
};
class c_R251 : public c_MethodInfo{
	public:
	c_R251();
	c_R251* m_new();
	void mark();
};
class c_R252 : public c_MethodInfo{
	public:
	c_R252();
	c_R252* m_new();
	void mark();
};
class c_R253 : public c_MethodInfo{
	public:
	c_R253();
	c_R253* m_new();
	void mark();
};
class c_R254 : public c_MethodInfo{
	public:
	c_R254();
	c_R254* m_new();
	void mark();
};
class c_R255 : public c_MethodInfo{
	public:
	c_R255();
	c_R255* m_new();
	void mark();
};
class c_R256 : public c_MethodInfo{
	public:
	c_R256();
	c_R256* m_new();
	void mark();
};
class c_R257 : public c_MethodInfo{
	public:
	c_R257();
	c_R257* m_new();
	void mark();
};
class c_R258 : public c_MethodInfo{
	public:
	c_R258();
	c_R258* m_new();
	void mark();
};
class c_R259 : public c_MethodInfo{
	public:
	c_R259();
	c_R259* m_new();
	void mark();
};
class c_R260 : public c_MethodInfo{
	public:
	c_R260();
	c_R260* m_new();
	void mark();
};
class c_R229 : public c_FunctionInfo{
	public:
	c_R229();
	c_R229* m_new();
	void mark();
};
class c_R230 : public c_FunctionInfo{
	public:
	c_R230();
	c_R230* m_new();
	void mark();
};
class c_R264 : public c_MethodInfo{
	public:
	c_R264();
	c_R264* m_new();
	void mark();
};
class c_R265 : public c_MethodInfo{
	public:
	c_R265();
	c_R265* m_new();
	void mark();
};
class c_R263 : public c_FunctionInfo{
	public:
	c_R263();
	c_R263* m_new();
	void mark();
};
class c_R266 : public c_FunctionInfo{
	public:
	c_R266();
	c_R266* m_new();
	void mark();
};
class c_R273 : public c_FieldInfo{
	public:
	c_R273();
	c_R273* m_new();
	void mark();
};
class c_R274 : public c_FieldInfo{
	public:
	c_R274();
	c_R274* m_new();
	void mark();
};
class c_R275 : public c_FieldInfo{
	public:
	c_R275();
	c_R275* m_new();
	void mark();
};
class c_R269 : public c_MethodInfo{
	public:
	c_R269();
	c_R269* m_new();
	void mark();
};
class c_R270 : public c_MethodInfo{
	public:
	c_R270();
	c_R270* m_new();
	void mark();
};
class c_R271 : public c_MethodInfo{
	public:
	c_R271();
	c_R271* m_new();
	void mark();
};
class c_R272 : public c_MethodInfo{
	public:
	c_R272();
	c_R272* m_new();
	void mark();
};
class c_R276 : public c_MethodInfo{
	public:
	c_R276();
	c_R276* m_new();
	void mark();
};
class c_R268 : public c_FunctionInfo{
	public:
	c_R268();
	c_R268* m_new();
	void mark();
};
class c_R277 : public c_FunctionInfo{
	public:
	c_R277();
	c_R277* m_new();
	void mark();
};
class c_R280 : public c_MethodInfo{
	public:
	c_R280();
	c_R280* m_new();
	void mark();
};
class c_R279 : public c_FunctionInfo{
	public:
	c_R279();
	c_R279* m_new();
	void mark();
};
class c_R314 : public c_FieldInfo{
	public:
	c_R314();
	c_R314* m_new();
	void mark();
};
class c_R284 : public c_MethodInfo{
	public:
	c_R284();
	c_R284* m_new();
	void mark();
};
class c_R285 : public c_MethodInfo{
	public:
	c_R285();
	c_R285* m_new();
	void mark();
};
class c_R286 : public c_MethodInfo{
	public:
	c_R286();
	c_R286* m_new();
	void mark();
};
class c_R287 : public c_MethodInfo{
	public:
	c_R287();
	c_R287* m_new();
	void mark();
};
class c_R288 : public c_MethodInfo{
	public:
	c_R288();
	c_R288* m_new();
	void mark();
};
class c_R289 : public c_MethodInfo{
	public:
	c_R289();
	c_R289* m_new();
	void mark();
};
class c_R290 : public c_MethodInfo{
	public:
	c_R290();
	c_R290* m_new();
	void mark();
};
class c_R291 : public c_MethodInfo{
	public:
	c_R291();
	c_R291* m_new();
	void mark();
};
class c_R292 : public c_MethodInfo{
	public:
	c_R292();
	c_R292* m_new();
	void mark();
};
class c_R293 : public c_MethodInfo{
	public:
	c_R293();
	c_R293* m_new();
	void mark();
};
class c_R294 : public c_MethodInfo{
	public:
	c_R294();
	c_R294* m_new();
	void mark();
};
class c_R295 : public c_MethodInfo{
	public:
	c_R295();
	c_R295* m_new();
	void mark();
};
class c_R296 : public c_MethodInfo{
	public:
	c_R296();
	c_R296* m_new();
	void mark();
};
class c_R297 : public c_MethodInfo{
	public:
	c_R297();
	c_R297* m_new();
	void mark();
};
class c_R298 : public c_MethodInfo{
	public:
	c_R298();
	c_R298* m_new();
	void mark();
};
class c_R299 : public c_MethodInfo{
	public:
	c_R299();
	c_R299* m_new();
	void mark();
};
class c_R300 : public c_MethodInfo{
	public:
	c_R300();
	c_R300* m_new();
	void mark();
};
class c_R301 : public c_MethodInfo{
	public:
	c_R301();
	c_R301* m_new();
	void mark();
};
class c_R302 : public c_MethodInfo{
	public:
	c_R302();
	c_R302* m_new();
	void mark();
};
class c_R303 : public c_MethodInfo{
	public:
	c_R303();
	c_R303* m_new();
	void mark();
};
class c_R304 : public c_MethodInfo{
	public:
	c_R304();
	c_R304* m_new();
	void mark();
};
class c_R305 : public c_MethodInfo{
	public:
	c_R305();
	c_R305* m_new();
	void mark();
};
class c_R306 : public c_MethodInfo{
	public:
	c_R306();
	c_R306* m_new();
	void mark();
};
class c_R307 : public c_MethodInfo{
	public:
	c_R307();
	c_R307* m_new();
	void mark();
};
class c_R308 : public c_MethodInfo{
	public:
	c_R308();
	c_R308* m_new();
	void mark();
};
class c_R309 : public c_MethodInfo{
	public:
	c_R309();
	c_R309* m_new();
	void mark();
};
class c_R310 : public c_MethodInfo{
	public:
	c_R310();
	c_R310* m_new();
	void mark();
};
class c_R311 : public c_MethodInfo{
	public:
	c_R311();
	c_R311* m_new();
	void mark();
};
class c_R312 : public c_MethodInfo{
	public:
	c_R312();
	c_R312* m_new();
	void mark();
};
class c_R313 : public c_MethodInfo{
	public:
	c_R313();
	c_R313* m_new();
	void mark();
};
class c_R282 : public c_FunctionInfo{
	public:
	c_R282();
	c_R282* m_new();
	void mark();
};
class c_R283 : public c_FunctionInfo{
	public:
	c_R283();
	c_R283* m_new();
	void mark();
};
class c_R317 : public c_MethodInfo{
	public:
	c_R317();
	c_R317* m_new();
	void mark();
};
class c_R318 : public c_MethodInfo{
	public:
	c_R318();
	c_R318* m_new();
	void mark();
};
class c_R319 : public c_MethodInfo{
	public:
	c_R319();
	c_R319* m_new();
	void mark();
};
class c_R316 : public c_FunctionInfo{
	public:
	c_R316();
	c_R316* m_new();
	void mark();
};
class c_R320 : public c_FunctionInfo{
	public:
	c_R320();
	c_R320* m_new();
	void mark();
};
class c_R327 : public c_FieldInfo{
	public:
	c_R327();
	c_R327* m_new();
	void mark();
};
class c_R328 : public c_FieldInfo{
	public:
	c_R328();
	c_R328* m_new();
	void mark();
};
class c_R329 : public c_FieldInfo{
	public:
	c_R329();
	c_R329* m_new();
	void mark();
};
class c_R323 : public c_MethodInfo{
	public:
	c_R323();
	c_R323* m_new();
	void mark();
};
class c_R324 : public c_MethodInfo{
	public:
	c_R324();
	c_R324* m_new();
	void mark();
};
class c_R325 : public c_MethodInfo{
	public:
	c_R325();
	c_R325* m_new();
	void mark();
};
class c_R326 : public c_MethodInfo{
	public:
	c_R326();
	c_R326* m_new();
	void mark();
};
class c_R330 : public c_MethodInfo{
	public:
	c_R330();
	c_R330* m_new();
	void mark();
};
class c_R322 : public c_FunctionInfo{
	public:
	c_R322();
	c_R322* m_new();
	void mark();
};
class c_R331 : public c_FunctionInfo{
	public:
	c_R331();
	c_R331* m_new();
	void mark();
};
class c_R334 : public c_MethodInfo{
	public:
	c_R334();
	c_R334* m_new();
	void mark();
};
class c_R333 : public c_FunctionInfo{
	public:
	c_R333();
	c_R333* m_new();
	void mark();
};
class c_R339 : public c_FieldInfo{
	public:
	c_R339();
	c_R339* m_new();
	void mark();
};
class c_R340 : public c_FieldInfo{
	public:
	c_R340();
	c_R340* m_new();
	void mark();
};
class c_R337 : public c_MethodInfo{
	public:
	c_R337();
	c_R337* m_new();
	void mark();
};
class c_R338 : public c_MethodInfo{
	public:
	c_R338();
	c_R338* m_new();
	void mark();
};
class c_R336 : public c_FunctionInfo{
	public:
	c_R336();
	c_R336* m_new();
	void mark();
};
class c_R341 : public c_FunctionInfo{
	public:
	c_R341();
	c_R341* m_new();
	void mark();
};
class c_R351 : public c_FieldInfo{
	public:
	c_R351();
	c_R351* m_new();
	void mark();
};
class c_R344 : public c_MethodInfo{
	public:
	c_R344();
	c_R344* m_new();
	void mark();
};
class c_R345 : public c_MethodInfo{
	public:
	c_R345();
	c_R345* m_new();
	void mark();
};
class c_R346 : public c_MethodInfo{
	public:
	c_R346();
	c_R346* m_new();
	void mark();
};
class c_R347 : public c_MethodInfo{
	public:
	c_R347();
	c_R347* m_new();
	void mark();
};
class c_R348 : public c_MethodInfo{
	public:
	c_R348();
	c_R348* m_new();
	void mark();
};
class c_R349 : public c_MethodInfo{
	public:
	c_R349();
	c_R349* m_new();
	void mark();
};
class c_R350 : public c_MethodInfo{
	public:
	c_R350();
	c_R350* m_new();
	void mark();
};
class c_R343 : public c_FunctionInfo{
	public:
	c_R343();
	c_R343* m_new();
	void mark();
};
class c_R352 : public c_FunctionInfo{
	public:
	c_R352();
	c_R352* m_new();
	void mark();
};
class c_R354 : public c_FunctionInfo{
	public:
	c_R354();
	c_R354* m_new();
	void mark();
};
class c_R379 : public c_FieldInfo{
	public:
	c_R379();
	c_R379* m_new();
	void mark();
};
class c_R356 : public c_MethodInfo{
	public:
	c_R356();
	c_R356* m_new();
	void mark();
};
class c_R357 : public c_MethodInfo{
	public:
	c_R357();
	c_R357* m_new();
	void mark();
};
class c_R358 : public c_MethodInfo{
	public:
	c_R358();
	c_R358* m_new();
	void mark();
};
class c_R359 : public c_MethodInfo{
	public:
	c_R359();
	c_R359* m_new();
	void mark();
};
class c_R360 : public c_MethodInfo{
	public:
	c_R360();
	c_R360* m_new();
	void mark();
};
class c_R361 : public c_MethodInfo{
	public:
	c_R361();
	c_R361* m_new();
	void mark();
};
class c_R362 : public c_MethodInfo{
	public:
	c_R362();
	c_R362* m_new();
	void mark();
};
class c_R363 : public c_MethodInfo{
	public:
	c_R363();
	c_R363* m_new();
	void mark();
};
class c_R364 : public c_MethodInfo{
	public:
	c_R364();
	c_R364* m_new();
	void mark();
};
class c_R365 : public c_MethodInfo{
	public:
	c_R365();
	c_R365* m_new();
	void mark();
};
class c_R366 : public c_MethodInfo{
	public:
	c_R366();
	c_R366* m_new();
	void mark();
};
class c_R367 : public c_MethodInfo{
	public:
	c_R367();
	c_R367* m_new();
	void mark();
};
class c_R368 : public c_MethodInfo{
	public:
	c_R368();
	c_R368* m_new();
	void mark();
};
class c_R369 : public c_MethodInfo{
	public:
	c_R369();
	c_R369* m_new();
	void mark();
};
class c_R370 : public c_MethodInfo{
	public:
	c_R370();
	c_R370* m_new();
	void mark();
};
class c_R371 : public c_MethodInfo{
	public:
	c_R371();
	c_R371* m_new();
	void mark();
};
class c_R372 : public c_MethodInfo{
	public:
	c_R372();
	c_R372* m_new();
	void mark();
};
class c_R373 : public c_MethodInfo{
	public:
	c_R373();
	c_R373* m_new();
	void mark();
};
class c_R374 : public c_MethodInfo{
	public:
	c_R374();
	c_R374* m_new();
	void mark();
};
class c_R375 : public c_MethodInfo{
	public:
	c_R375();
	c_R375* m_new();
	void mark();
};
class c_R376 : public c_MethodInfo{
	public:
	c_R376();
	c_R376* m_new();
	void mark();
};
class c_R377 : public c_MethodInfo{
	public:
	c_R377();
	c_R377* m_new();
	void mark();
};
class c_R378 : public c_MethodInfo{
	public:
	c_R378();
	c_R378* m_new();
	void mark();
};
class c_R380 : public c_FunctionInfo{
	public:
	c_R380();
	c_R380* m_new();
	void mark();
};
class c_R382 : public c_MethodInfo{
	public:
	c_R382();
	c_R382* m_new();
	void mark();
};
class c_R383 : public c_FunctionInfo{
	public:
	c_R383();
	c_R383* m_new();
	void mark();
};
class c_R393 : public c_FieldInfo{
	public:
	c_R393();
	c_R393* m_new();
	void mark();
};
class c_R386 : public c_MethodInfo{
	public:
	c_R386();
	c_R386* m_new();
	void mark();
};
class c_R387 : public c_MethodInfo{
	public:
	c_R387();
	c_R387* m_new();
	void mark();
};
class c_R388 : public c_MethodInfo{
	public:
	c_R388();
	c_R388* m_new();
	void mark();
};
class c_R389 : public c_MethodInfo{
	public:
	c_R389();
	c_R389* m_new();
	void mark();
};
class c_R390 : public c_MethodInfo{
	public:
	c_R390();
	c_R390* m_new();
	void mark();
};
class c_R391 : public c_MethodInfo{
	public:
	c_R391();
	c_R391* m_new();
	void mark();
};
class c_R392 : public c_MethodInfo{
	public:
	c_R392();
	c_R392* m_new();
	void mark();
};
class c_R385 : public c_FunctionInfo{
	public:
	c_R385();
	c_R385* m_new();
	void mark();
};
class c_R394 : public c_FunctionInfo{
	public:
	c_R394();
	c_R394* m_new();
	void mark();
};
class c_R396 : public c_FunctionInfo{
	public:
	c_R396();
	c_R396* m_new();
	void mark();
};
class c_R421 : public c_FieldInfo{
	public:
	c_R421();
	c_R421* m_new();
	void mark();
};
class c_R398 : public c_MethodInfo{
	public:
	c_R398();
	c_R398* m_new();
	void mark();
};
class c_R399 : public c_MethodInfo{
	public:
	c_R399();
	c_R399* m_new();
	void mark();
};
class c_R400 : public c_MethodInfo{
	public:
	c_R400();
	c_R400* m_new();
	void mark();
};
class c_R401 : public c_MethodInfo{
	public:
	c_R401();
	c_R401* m_new();
	void mark();
};
class c_R402 : public c_MethodInfo{
	public:
	c_R402();
	c_R402* m_new();
	void mark();
};
class c_R403 : public c_MethodInfo{
	public:
	c_R403();
	c_R403* m_new();
	void mark();
};
class c_R404 : public c_MethodInfo{
	public:
	c_R404();
	c_R404* m_new();
	void mark();
};
class c_R405 : public c_MethodInfo{
	public:
	c_R405();
	c_R405* m_new();
	void mark();
};
class c_R406 : public c_MethodInfo{
	public:
	c_R406();
	c_R406* m_new();
	void mark();
};
class c_R407 : public c_MethodInfo{
	public:
	c_R407();
	c_R407* m_new();
	void mark();
};
class c_R408 : public c_MethodInfo{
	public:
	c_R408();
	c_R408* m_new();
	void mark();
};
class c_R409 : public c_MethodInfo{
	public:
	c_R409();
	c_R409* m_new();
	void mark();
};
class c_R410 : public c_MethodInfo{
	public:
	c_R410();
	c_R410* m_new();
	void mark();
};
class c_R411 : public c_MethodInfo{
	public:
	c_R411();
	c_R411* m_new();
	void mark();
};
class c_R412 : public c_MethodInfo{
	public:
	c_R412();
	c_R412* m_new();
	void mark();
};
class c_R413 : public c_MethodInfo{
	public:
	c_R413();
	c_R413* m_new();
	void mark();
};
class c_R414 : public c_MethodInfo{
	public:
	c_R414();
	c_R414* m_new();
	void mark();
};
class c_R415 : public c_MethodInfo{
	public:
	c_R415();
	c_R415* m_new();
	void mark();
};
class c_R416 : public c_MethodInfo{
	public:
	c_R416();
	c_R416* m_new();
	void mark();
};
class c_R417 : public c_MethodInfo{
	public:
	c_R417();
	c_R417* m_new();
	void mark();
};
class c_R418 : public c_MethodInfo{
	public:
	c_R418();
	c_R418* m_new();
	void mark();
};
class c_R419 : public c_MethodInfo{
	public:
	c_R419();
	c_R419* m_new();
	void mark();
};
class c_R420 : public c_MethodInfo{
	public:
	c_R420();
	c_R420* m_new();
	void mark();
};
class c_R422 : public c_FunctionInfo{
	public:
	c_R422();
	c_R422* m_new();
	void mark();
};
class c_R424 : public c_MethodInfo{
	public:
	c_R424();
	c_R424* m_new();
	void mark();
};
class c_R425 : public c_FunctionInfo{
	public:
	c_R425();
	c_R425* m_new();
	void mark();
};
class c_R435 : public c_FieldInfo{
	public:
	c_R435();
	c_R435* m_new();
	void mark();
};
class c_R428 : public c_MethodInfo{
	public:
	c_R428();
	c_R428* m_new();
	void mark();
};
class c_R429 : public c_MethodInfo{
	public:
	c_R429();
	c_R429* m_new();
	void mark();
};
class c_R430 : public c_MethodInfo{
	public:
	c_R430();
	c_R430* m_new();
	void mark();
};
class c_R431 : public c_MethodInfo{
	public:
	c_R431();
	c_R431* m_new();
	void mark();
};
class c_R432 : public c_MethodInfo{
	public:
	c_R432();
	c_R432* m_new();
	void mark();
};
class c_R433 : public c_MethodInfo{
	public:
	c_R433();
	c_R433* m_new();
	void mark();
};
class c_R434 : public c_MethodInfo{
	public:
	c_R434();
	c_R434* m_new();
	void mark();
};
class c_R427 : public c_FunctionInfo{
	public:
	c_R427();
	c_R427* m_new();
	void mark();
};
class c_R436 : public c_FunctionInfo{
	public:
	c_R436();
	c_R436* m_new();
	void mark();
};
class c_R438 : public c_FunctionInfo{
	public:
	c_R438();
	c_R438* m_new();
	void mark();
};
class c_R463 : public c_FieldInfo{
	public:
	c_R463();
	c_R463* m_new();
	void mark();
};
class c_R440 : public c_MethodInfo{
	public:
	c_R440();
	c_R440* m_new();
	void mark();
};
class c_R441 : public c_MethodInfo{
	public:
	c_R441();
	c_R441* m_new();
	void mark();
};
class c_R442 : public c_MethodInfo{
	public:
	c_R442();
	c_R442* m_new();
	void mark();
};
class c_R443 : public c_MethodInfo{
	public:
	c_R443();
	c_R443* m_new();
	void mark();
};
class c_R444 : public c_MethodInfo{
	public:
	c_R444();
	c_R444* m_new();
	void mark();
};
class c_R445 : public c_MethodInfo{
	public:
	c_R445();
	c_R445* m_new();
	void mark();
};
class c_R446 : public c_MethodInfo{
	public:
	c_R446();
	c_R446* m_new();
	void mark();
};
class c_R447 : public c_MethodInfo{
	public:
	c_R447();
	c_R447* m_new();
	void mark();
};
class c_R448 : public c_MethodInfo{
	public:
	c_R448();
	c_R448* m_new();
	void mark();
};
class c_R449 : public c_MethodInfo{
	public:
	c_R449();
	c_R449* m_new();
	void mark();
};
class c_R450 : public c_MethodInfo{
	public:
	c_R450();
	c_R450* m_new();
	void mark();
};
class c_R451 : public c_MethodInfo{
	public:
	c_R451();
	c_R451* m_new();
	void mark();
};
class c_R452 : public c_MethodInfo{
	public:
	c_R452();
	c_R452* m_new();
	void mark();
};
class c_R453 : public c_MethodInfo{
	public:
	c_R453();
	c_R453* m_new();
	void mark();
};
class c_R454 : public c_MethodInfo{
	public:
	c_R454();
	c_R454* m_new();
	void mark();
};
class c_R455 : public c_MethodInfo{
	public:
	c_R455();
	c_R455* m_new();
	void mark();
};
class c_R456 : public c_MethodInfo{
	public:
	c_R456();
	c_R456* m_new();
	void mark();
};
class c_R457 : public c_MethodInfo{
	public:
	c_R457();
	c_R457* m_new();
	void mark();
};
class c_R458 : public c_MethodInfo{
	public:
	c_R458();
	c_R458* m_new();
	void mark();
};
class c_R459 : public c_MethodInfo{
	public:
	c_R459();
	c_R459* m_new();
	void mark();
};
class c_R460 : public c_MethodInfo{
	public:
	c_R460();
	c_R460* m_new();
	void mark();
};
class c_R461 : public c_MethodInfo{
	public:
	c_R461();
	c_R461* m_new();
	void mark();
};
class c_R462 : public c_MethodInfo{
	public:
	c_R462();
	c_R462* m_new();
	void mark();
};
class c_R464 : public c_FunctionInfo{
	public:
	c_R464();
	c_R464* m_new();
	void mark();
};
class c_R466 : public c_MethodInfo{
	public:
	c_R466();
	c_R466* m_new();
	void mark();
};
class c_R467 : public c_FunctionInfo{
	public:
	c_R467();
	c_R467* m_new();
	void mark();
};
class c_R497 : public c_GlobalInfo{
	public:
	c_R497();
	c_R497* m_new();
	void mark();
};
class c_R498 : public c_FieldInfo{
	public:
	c_R498();
	c_R498* m_new();
	void mark();
};
class c_R499 : public c_FieldInfo{
	public:
	c_R499();
	c_R499* m_new();
	void mark();
};
class c_R471 : public c_MethodInfo{
	public:
	c_R471();
	c_R471* m_new();
	void mark();
};
class c_R472 : public c_MethodInfo{
	public:
	c_R472();
	c_R472* m_new();
	void mark();
};
class c_R473 : public c_MethodInfo{
	public:
	c_R473();
	c_R473* m_new();
	void mark();
};
class c_R474 : public c_MethodInfo{
	public:
	c_R474();
	c_R474* m_new();
	void mark();
};
class c_R475 : public c_MethodInfo{
	public:
	c_R475();
	c_R475* m_new();
	void mark();
};
class c_R476 : public c_MethodInfo{
	public:
	c_R476();
	c_R476* m_new();
	void mark();
};
class c_R477 : public c_MethodInfo{
	public:
	c_R477();
	c_R477* m_new();
	void mark();
};
class c_R478 : public c_MethodInfo{
	public:
	c_R478();
	c_R478* m_new();
	void mark();
};
class c_R479 : public c_MethodInfo{
	public:
	c_R479();
	c_R479* m_new();
	void mark();
};
class c_R480 : public c_MethodInfo{
	public:
	c_R480();
	c_R480* m_new();
	void mark();
};
class c_R481 : public c_MethodInfo{
	public:
	c_R481();
	c_R481* m_new();
	void mark();
};
class c_R482 : public c_MethodInfo{
	public:
	c_R482();
	c_R482* m_new();
	void mark();
};
class c_R483 : public c_MethodInfo{
	public:
	c_R483();
	c_R483* m_new();
	void mark();
};
class c_R484 : public c_MethodInfo{
	public:
	c_R484();
	c_R484* m_new();
	void mark();
};
class c_R485 : public c_MethodInfo{
	public:
	c_R485();
	c_R485* m_new();
	void mark();
};
class c_R486 : public c_MethodInfo{
	public:
	c_R486();
	c_R486* m_new();
	void mark();
};
class c_R487 : public c_MethodInfo{
	public:
	c_R487();
	c_R487* m_new();
	void mark();
};
class c_R488 : public c_MethodInfo{
	public:
	c_R488();
	c_R488* m_new();
	void mark();
};
class c_R489 : public c_MethodInfo{
	public:
	c_R489();
	c_R489* m_new();
	void mark();
};
class c_R490 : public c_MethodInfo{
	public:
	c_R490();
	c_R490* m_new();
	void mark();
};
class c_R491 : public c_MethodInfo{
	public:
	c_R491();
	c_R491* m_new();
	void mark();
};
class c_R492 : public c_MethodInfo{
	public:
	c_R492();
	c_R492* m_new();
	void mark();
};
class c_R493 : public c_MethodInfo{
	public:
	c_R493();
	c_R493* m_new();
	void mark();
};
class c_R494 : public c_MethodInfo{
	public:
	c_R494();
	c_R494* m_new();
	void mark();
};
class c_R495 : public c_MethodInfo{
	public:
	c_R495();
	c_R495* m_new();
	void mark();
};
class c_R496 : public c_MethodInfo{
	public:
	c_R496();
	c_R496* m_new();
	void mark();
};
class c_R500 : public c_MethodInfo{
	public:
	c_R500();
	c_R500* m_new();
	void mark();
};
class c_R501 : public c_MethodInfo{
	public:
	c_R501();
	c_R501* m_new();
	void mark();
};
class c_R502 : public c_MethodInfo{
	public:
	c_R502();
	c_R502* m_new();
	void mark();
};
class c_R503 : public c_MethodInfo{
	public:
	c_R503();
	c_R503* m_new();
	void mark();
};
class c_R504 : public c_MethodInfo{
	public:
	c_R504();
	c_R504* m_new();
	void mark();
};
class c_R469 : public c_FunctionInfo{
	public:
	c_R469();
	c_R469* m_new();
	void mark();
};
class c_R470 : public c_FunctionInfo{
	public:
	c_R470();
	c_R470* m_new();
	void mark();
};
class c_R507 : public c_MethodInfo{
	public:
	c_R507();
	c_R507* m_new();
	void mark();
};
class c_R508 : public c_MethodInfo{
	public:
	c_R508();
	c_R508* m_new();
	void mark();
};
class c_R506 : public c_FunctionInfo{
	public:
	c_R506();
	c_R506* m_new();
	void mark();
};
class c_R509 : public c_FunctionInfo{
	public:
	c_R509();
	c_R509* m_new();
	void mark();
};
class c_R539 : public c_GlobalInfo{
	public:
	c_R539();
	c_R539* m_new();
	void mark();
};
class c_R540 : public c_FieldInfo{
	public:
	c_R540();
	c_R540* m_new();
	void mark();
};
class c_R541 : public c_FieldInfo{
	public:
	c_R541();
	c_R541* m_new();
	void mark();
};
class c_R513 : public c_MethodInfo{
	public:
	c_R513();
	c_R513* m_new();
	void mark();
};
class c_R514 : public c_MethodInfo{
	public:
	c_R514();
	c_R514* m_new();
	void mark();
};
class c_R515 : public c_MethodInfo{
	public:
	c_R515();
	c_R515* m_new();
	void mark();
};
class c_R516 : public c_MethodInfo{
	public:
	c_R516();
	c_R516* m_new();
	void mark();
};
class c_R517 : public c_MethodInfo{
	public:
	c_R517();
	c_R517* m_new();
	void mark();
};
class c_R518 : public c_MethodInfo{
	public:
	c_R518();
	c_R518* m_new();
	void mark();
};
class c_R519 : public c_MethodInfo{
	public:
	c_R519();
	c_R519* m_new();
	void mark();
};
class c_R520 : public c_MethodInfo{
	public:
	c_R520();
	c_R520* m_new();
	void mark();
};
class c_R521 : public c_MethodInfo{
	public:
	c_R521();
	c_R521* m_new();
	void mark();
};
class c_R522 : public c_MethodInfo{
	public:
	c_R522();
	c_R522* m_new();
	void mark();
};
class c_R523 : public c_MethodInfo{
	public:
	c_R523();
	c_R523* m_new();
	void mark();
};
class c_R524 : public c_MethodInfo{
	public:
	c_R524();
	c_R524* m_new();
	void mark();
};
class c_R525 : public c_MethodInfo{
	public:
	c_R525();
	c_R525* m_new();
	void mark();
};
class c_R526 : public c_MethodInfo{
	public:
	c_R526();
	c_R526* m_new();
	void mark();
};
class c_R527 : public c_MethodInfo{
	public:
	c_R527();
	c_R527* m_new();
	void mark();
};
class c_R528 : public c_MethodInfo{
	public:
	c_R528();
	c_R528* m_new();
	void mark();
};
class c_R529 : public c_MethodInfo{
	public:
	c_R529();
	c_R529* m_new();
	void mark();
};
class c_R530 : public c_MethodInfo{
	public:
	c_R530();
	c_R530* m_new();
	void mark();
};
class c_R531 : public c_MethodInfo{
	public:
	c_R531();
	c_R531* m_new();
	void mark();
};
class c_R532 : public c_MethodInfo{
	public:
	c_R532();
	c_R532* m_new();
	void mark();
};
class c_R533 : public c_MethodInfo{
	public:
	c_R533();
	c_R533* m_new();
	void mark();
};
class c_R534 : public c_MethodInfo{
	public:
	c_R534();
	c_R534* m_new();
	void mark();
};
class c_R535 : public c_MethodInfo{
	public:
	c_R535();
	c_R535* m_new();
	void mark();
};
class c_R536 : public c_MethodInfo{
	public:
	c_R536();
	c_R536* m_new();
	void mark();
};
class c_R537 : public c_MethodInfo{
	public:
	c_R537();
	c_R537* m_new();
	void mark();
};
class c_R538 : public c_MethodInfo{
	public:
	c_R538();
	c_R538* m_new();
	void mark();
};
class c_R542 : public c_MethodInfo{
	public:
	c_R542();
	c_R542* m_new();
	void mark();
};
class c_R543 : public c_MethodInfo{
	public:
	c_R543();
	c_R543* m_new();
	void mark();
};
class c_R544 : public c_MethodInfo{
	public:
	c_R544();
	c_R544* m_new();
	void mark();
};
class c_R545 : public c_MethodInfo{
	public:
	c_R545();
	c_R545* m_new();
	void mark();
};
class c_R546 : public c_MethodInfo{
	public:
	c_R546();
	c_R546* m_new();
	void mark();
};
class c_R511 : public c_FunctionInfo{
	public:
	c_R511();
	c_R511* m_new();
	void mark();
};
class c_R512 : public c_FunctionInfo{
	public:
	c_R512();
	c_R512* m_new();
	void mark();
};
class c_R549 : public c_MethodInfo{
	public:
	c_R549();
	c_R549* m_new();
	void mark();
};
class c_R550 : public c_MethodInfo{
	public:
	c_R550();
	c_R550* m_new();
	void mark();
};
class c_R548 : public c_FunctionInfo{
	public:
	c_R548();
	c_R548* m_new();
	void mark();
};
class c_R551 : public c_FunctionInfo{
	public:
	c_R551();
	c_R551* m_new();
	void mark();
};
class c_R581 : public c_GlobalInfo{
	public:
	c_R581();
	c_R581* m_new();
	void mark();
};
class c_R582 : public c_FieldInfo{
	public:
	c_R582();
	c_R582* m_new();
	void mark();
};
class c_R583 : public c_FieldInfo{
	public:
	c_R583();
	c_R583* m_new();
	void mark();
};
class c_R555 : public c_MethodInfo{
	public:
	c_R555();
	c_R555* m_new();
	void mark();
};
class c_R556 : public c_MethodInfo{
	public:
	c_R556();
	c_R556* m_new();
	void mark();
};
class c_R557 : public c_MethodInfo{
	public:
	c_R557();
	c_R557* m_new();
	void mark();
};
class c_R558 : public c_MethodInfo{
	public:
	c_R558();
	c_R558* m_new();
	void mark();
};
class c_R559 : public c_MethodInfo{
	public:
	c_R559();
	c_R559* m_new();
	void mark();
};
class c_R560 : public c_MethodInfo{
	public:
	c_R560();
	c_R560* m_new();
	void mark();
};
class c_R561 : public c_MethodInfo{
	public:
	c_R561();
	c_R561* m_new();
	void mark();
};
class c_R562 : public c_MethodInfo{
	public:
	c_R562();
	c_R562* m_new();
	void mark();
};
class c_R563 : public c_MethodInfo{
	public:
	c_R563();
	c_R563* m_new();
	void mark();
};
class c_R564 : public c_MethodInfo{
	public:
	c_R564();
	c_R564* m_new();
	void mark();
};
class c_R565 : public c_MethodInfo{
	public:
	c_R565();
	c_R565* m_new();
	void mark();
};
class c_R566 : public c_MethodInfo{
	public:
	c_R566();
	c_R566* m_new();
	void mark();
};
class c_R567 : public c_MethodInfo{
	public:
	c_R567();
	c_R567* m_new();
	void mark();
};
class c_R568 : public c_MethodInfo{
	public:
	c_R568();
	c_R568* m_new();
	void mark();
};
class c_R569 : public c_MethodInfo{
	public:
	c_R569();
	c_R569* m_new();
	void mark();
};
class c_R570 : public c_MethodInfo{
	public:
	c_R570();
	c_R570* m_new();
	void mark();
};
class c_R571 : public c_MethodInfo{
	public:
	c_R571();
	c_R571* m_new();
	void mark();
};
class c_R572 : public c_MethodInfo{
	public:
	c_R572();
	c_R572* m_new();
	void mark();
};
class c_R573 : public c_MethodInfo{
	public:
	c_R573();
	c_R573* m_new();
	void mark();
};
class c_R574 : public c_MethodInfo{
	public:
	c_R574();
	c_R574* m_new();
	void mark();
};
class c_R575 : public c_MethodInfo{
	public:
	c_R575();
	c_R575* m_new();
	void mark();
};
class c_R576 : public c_MethodInfo{
	public:
	c_R576();
	c_R576* m_new();
	void mark();
};
class c_R577 : public c_MethodInfo{
	public:
	c_R577();
	c_R577* m_new();
	void mark();
};
class c_R578 : public c_MethodInfo{
	public:
	c_R578();
	c_R578* m_new();
	void mark();
};
class c_R579 : public c_MethodInfo{
	public:
	c_R579();
	c_R579* m_new();
	void mark();
};
class c_R580 : public c_MethodInfo{
	public:
	c_R580();
	c_R580* m_new();
	void mark();
};
class c_R584 : public c_MethodInfo{
	public:
	c_R584();
	c_R584* m_new();
	void mark();
};
class c_R585 : public c_MethodInfo{
	public:
	c_R585();
	c_R585* m_new();
	void mark();
};
class c_R586 : public c_MethodInfo{
	public:
	c_R586();
	c_R586* m_new();
	void mark();
};
class c_R587 : public c_MethodInfo{
	public:
	c_R587();
	c_R587* m_new();
	void mark();
};
class c_R588 : public c_MethodInfo{
	public:
	c_R588();
	c_R588* m_new();
	void mark();
};
class c_R553 : public c_FunctionInfo{
	public:
	c_R553();
	c_R553* m_new();
	void mark();
};
class c_R554 : public c_FunctionInfo{
	public:
	c_R554();
	c_R554* m_new();
	void mark();
};
class c_R591 : public c_MethodInfo{
	public:
	c_R591();
	c_R591* m_new();
	void mark();
};
class c_R592 : public c_MethodInfo{
	public:
	c_R592();
	c_R592* m_new();
	void mark();
};
class c_R593 : public c_MethodInfo{
	public:
	c_R593();
	c_R593* m_new();
	void mark();
};
class c_R590 : public c_FunctionInfo{
	public:
	c_R590();
	c_R590* m_new();
	void mark();
};
class c_R594 : public c_FunctionInfo{
	public:
	c_R594();
	c_R594* m_new();
	void mark();
};
class c_R596 : public c_FieldInfo{
	public:
	c_R596();
	c_R596* m_new();
	void mark();
};
class c_R597 : public c_FieldInfo{
	public:
	c_R597();
	c_R597* m_new();
	void mark();
};
class c_R598 : public c_FieldInfo{
	public:
	c_R598();
	c_R598* m_new();
	void mark();
};
class c_R599 : public c_FieldInfo{
	public:
	c_R599();
	c_R599* m_new();
	void mark();
};
class c_R600 : public c_FieldInfo{
	public:
	c_R600();
	c_R600* m_new();
	void mark();
};
class c_R606 : public c_GlobalInfo{
	public:
	c_R606();
	c_R606* m_new();
	void mark();
};
class c_R607 : public c_GlobalInfo{
	public:
	c_R607();
	c_R607* m_new();
	void mark();
};
class c_R608 : public c_GlobalInfo{
	public:
	c_R608();
	c_R608* m_new();
	void mark();
};
class c_R609 : public c_GlobalInfo{
	public:
	c_R609();
	c_R609* m_new();
	void mark();
};
class c_R610 : public c_GlobalInfo{
	public:
	c_R610();
	c_R610* m_new();
	void mark();
};
class c_R611 : public c_GlobalInfo{
	public:
	c_R611();
	c_R611* m_new();
	void mark();
};
class c_R612 : public c_GlobalInfo{
	public:
	c_R612();
	c_R612* m_new();
	void mark();
};
class c_R613 : public c_GlobalInfo{
	public:
	c_R613();
	c_R613* m_new();
	void mark();
};
class c_R614 : public c_GlobalInfo{
	public:
	c_R614();
	c_R614* m_new();
	void mark();
};
class c_R615 : public c_GlobalInfo{
	public:
	c_R615();
	c_R615* m_new();
	void mark();
};
class c_R616 : public c_GlobalInfo{
	public:
	c_R616();
	c_R616* m_new();
	void mark();
};
class c_R617 : public c_GlobalInfo{
	public:
	c_R617();
	c_R617* m_new();
	void mark();
};
class c_R618 : public c_GlobalInfo{
	public:
	c_R618();
	c_R618* m_new();
	void mark();
};
class c_R619 : public c_GlobalInfo{
	public:
	c_R619();
	c_R619* m_new();
	void mark();
};
class c_R620 : public c_GlobalInfo{
	public:
	c_R620();
	c_R620* m_new();
	void mark();
};
class c_R621 : public c_GlobalInfo{
	public:
	c_R621();
	c_R621* m_new();
	void mark();
};
class c_R622 : public c_GlobalInfo{
	public:
	c_R622();
	c_R622* m_new();
	void mark();
};
class c_R623 : public c_GlobalInfo{
	public:
	c_R623();
	c_R623* m_new();
	void mark();
};
class c_R624 : public c_GlobalInfo{
	public:
	c_R624();
	c_R624* m_new();
	void mark();
};
class c_R625 : public c_GlobalInfo{
	public:
	c_R625();
	c_R625* m_new();
	void mark();
};
class c_R626 : public c_GlobalInfo{
	public:
	c_R626();
	c_R626* m_new();
	void mark();
};
class c_R602 : public c_FieldInfo{
	public:
	c_R602();
	c_R602* m_new();
	void mark();
};
class c_R603 : public c_FieldInfo{
	public:
	c_R603();
	c_R603* m_new();
	void mark();
};
class c_R604 : public c_FieldInfo{
	public:
	c_R604();
	c_R604* m_new();
	void mark();
};
class c_R605 : public c_FieldInfo{
	public:
	c_R605();
	c_R605* m_new();
	void mark();
};
class c_R630 : public c_MethodInfo{
	public:
	c_R630();
	c_R630* m_new();
	void mark();
};
class c_R631 : public c_MethodInfo{
	public:
	c_R631();
	c_R631* m_new();
	void mark();
};
class c_R632 : public c_MethodInfo{
	public:
	c_R632();
	c_R632* m_new();
	void mark();
};
class c_R633 : public c_MethodInfo{
	public:
	c_R633();
	c_R633* m_new();
	void mark();
};
class c_R634 : public c_MethodInfo{
	public:
	c_R634();
	c_R634* m_new();
	void mark();
};
class c_R635 : public c_MethodInfo{
	public:
	c_R635();
	c_R635* m_new();
	void mark();
};
class c_R636 : public c_MethodInfo{
	public:
	c_R636();
	c_R636* m_new();
	void mark();
};
class c_R637 : public c_MethodInfo{
	public:
	c_R637();
	c_R637* m_new();
	void mark();
};
class c_R638 : public c_MethodInfo{
	public:
	c_R638();
	c_R638* m_new();
	void mark();
};
class c_R639 : public c_MethodInfo{
	public:
	c_R639();
	c_R639* m_new();
	void mark();
};
class c_R640 : public c_MethodInfo{
	public:
	c_R640();
	c_R640* m_new();
	void mark();
};
class c_R641 : public c_MethodInfo{
	public:
	c_R641();
	c_R641* m_new();
	void mark();
};
class c_R642 : public c_MethodInfo{
	public:
	c_R642();
	c_R642* m_new();
	void mark();
};
class c_R643 : public c_MethodInfo{
	public:
	c_R643();
	c_R643* m_new();
	void mark();
};
class c_R644 : public c_MethodInfo{
	public:
	c_R644();
	c_R644* m_new();
	void mark();
};
class c_R645 : public c_MethodInfo{
	public:
	c_R645();
	c_R645* m_new();
	void mark();
};
class c_R646 : public c_MethodInfo{
	public:
	c_R646();
	c_R646* m_new();
	void mark();
};
class c_R647 : public c_MethodInfo{
	public:
	c_R647();
	c_R647* m_new();
	void mark();
};
class c_R648 : public c_MethodInfo{
	public:
	c_R648();
	c_R648* m_new();
	void mark();
};
class c_R649 : public c_MethodInfo{
	public:
	c_R649();
	c_R649* m_new();
	void mark();
};
class c_R627 : public c_FunctionInfo{
	public:
	c_R627();
	c_R627* m_new();
	void mark();
};
class c_R628 : public c_FunctionInfo{
	public:
	c_R628();
	c_R628* m_new();
	void mark();
};
class c_R629 : public c_FunctionInfo{
	public:
	c_R629();
	c_R629* m_new();
	void mark();
};
class c_R650 : public c_FunctionInfo{
	public:
	c_R650();
	c_R650* m_new();
	void mark();
};
class c_R655 : public c_MethodInfo{
	public:
	c_R655();
	c_R655* m_new();
	void mark();
};
class c_R656 : public c_MethodInfo{
	public:
	c_R656();
	c_R656* m_new();
	void mark();
};
class c_R657 : public c_MethodInfo{
	public:
	c_R657();
	c_R657* m_new();
	void mark();
};
class c_R658 : public c_MethodInfo{
	public:
	c_R658();
	c_R658* m_new();
	void mark();
};
class c_R659 : public c_MethodInfo{
	public:
	c_R659();
	c_R659* m_new();
	void mark();
};
class c_R660 : public c_MethodInfo{
	public:
	c_R660();
	c_R660* m_new();
	void mark();
};
class c_R661 : public c_MethodInfo{
	public:
	c_R661();
	c_R661* m_new();
	void mark();
};
class c_R662 : public c_MethodInfo{
	public:
	c_R662();
	c_R662* m_new();
	void mark();
};
class c_R663 : public c_MethodInfo{
	public:
	c_R663();
	c_R663* m_new();
	void mark();
};
class c_R664 : public c_MethodInfo{
	public:
	c_R664();
	c_R664* m_new();
	void mark();
};
class c_R652 : public c_FunctionInfo{
	public:
	c_R652();
	c_R652* m_new();
	void mark();
};
class c_R653 : public c_FunctionInfo{
	public:
	c_R653();
	c_R653* m_new();
	void mark();
};
class c_R654 : public c_FunctionInfo{
	public:
	c_R654();
	c_R654* m_new();
	void mark();
};
class c_R666 : public c_FieldInfo{
	public:
	c_R666();
	c_R666* m_new();
	void mark();
};
class c_R667 : public c_FieldInfo{
	public:
	c_R667();
	c_R667* m_new();
	void mark();
};
class c_R670 : public c_MethodInfo{
	public:
	c_R670();
	c_R670* m_new();
	void mark();
};
class c_R672 : public c_MethodInfo{
	public:
	c_R672();
	c_R672* m_new();
	void mark();
};
class c_R673 : public c_MethodInfo{
	public:
	c_R673();
	c_R673* m_new();
	void mark();
};
class c_R674 : public c_MethodInfo{
	public:
	c_R674();
	c_R674* m_new();
	void mark();
};
class c_R675 : public c_MethodInfo{
	public:
	c_R675();
	c_R675* m_new();
	void mark();
};
class c_R676 : public c_MethodInfo{
	public:
	c_R676();
	c_R676* m_new();
	void mark();
};
class c_R677 : public c_MethodInfo{
	public:
	c_R677();
	c_R677* m_new();
	void mark();
};
class c_R678 : public c_MethodInfo{
	public:
	c_R678();
	c_R678* m_new();
	void mark();
};
class c_R679 : public c_MethodInfo{
	public:
	c_R679();
	c_R679* m_new();
	void mark();
};
class c_R680 : public c_MethodInfo{
	public:
	c_R680();
	c_R680* m_new();
	void mark();
};
class c_R681 : public c_MethodInfo{
	public:
	c_R681();
	c_R681* m_new();
	void mark();
};
class c_R682 : public c_MethodInfo{
	public:
	c_R682();
	c_R682* m_new();
	void mark();
};
class c_R683 : public c_MethodInfo{
	public:
	c_R683();
	c_R683* m_new();
	void mark();
};
class c_R684 : public c_MethodInfo{
	public:
	c_R684();
	c_R684* m_new();
	void mark();
};
class c_R685 : public c_MethodInfo{
	public:
	c_R685();
	c_R685* m_new();
	void mark();
};
class c_R686 : public c_MethodInfo{
	public:
	c_R686();
	c_R686* m_new();
	void mark();
};
class c_R687 : public c_MethodInfo{
	public:
	c_R687();
	c_R687* m_new();
	void mark();
};
class c_R688 : public c_MethodInfo{
	public:
	c_R688();
	c_R688* m_new();
	void mark();
};
class c_R689 : public c_MethodInfo{
	public:
	c_R689();
	c_R689* m_new();
	void mark();
};
class c_R690 : public c_MethodInfo{
	public:
	c_R690();
	c_R690* m_new();
	void mark();
};
class c_R691 : public c_MethodInfo{
	public:
	c_R691();
	c_R691* m_new();
	void mark();
};
class c_R692 : public c_MethodInfo{
	public:
	c_R692();
	c_R692* m_new();
	void mark();
};
class c_R693 : public c_MethodInfo{
	public:
	c_R693();
	c_R693* m_new();
	void mark();
};
class c_R671 : public c_FunctionInfo{
	public:
	c_R671();
	c_R671* m_new();
	void mark();
};
class c_R694 : public c_FunctionInfo{
	public:
	c_R694();
	c_R694* m_new();
	void mark();
};
class c_R695 : public c_FunctionInfo{
	public:
	c_R695();
	c_R695* m_new();
	void mark();
};
class c_R696 : public c_FunctionInfo{
	public:
	c_R696();
	c_R696* m_new();
	void mark();
};
class c_R697 : public c_FunctionInfo{
	public:
	c_R697();
	c_R697* m_new();
	void mark();
};
class c_R698 : public c_FunctionInfo{
	public:
	c_R698();
	c_R698* m_new();
	void mark();
};
class c_R699 : public c_FunctionInfo{
	public:
	c_R699();
	c_R699* m_new();
	void mark();
};
class c_R700 : public c_FunctionInfo{
	public:
	c_R700();
	c_R700* m_new();
	void mark();
};
class c_R701 : public c_FunctionInfo{
	public:
	c_R701();
	c_R701* m_new();
	void mark();
};
class c_R702 : public c_FunctionInfo{
	public:
	c_R702();
	c_R702* m_new();
	void mark();
};
class c_R703 : public c_FunctionInfo{
	public:
	c_R703();
	c_R703* m_new();
	void mark();
};
class c_R668 : public c_FunctionInfo{
	public:
	c_R668();
	c_R668* m_new();
	void mark();
};
class c_R669 : public c_FunctionInfo{
	public:
	c_R669();
	c_R669* m_new();
	void mark();
};
class c_R704 : public c_FunctionInfo{
	public:
	c_R704();
	c_R704* m_new();
	void mark();
};
class c_R706 : public c_FieldInfo{
	public:
	c_R706();
	c_R706* m_new();
	void mark();
};
class c_R707 : public c_FieldInfo{
	public:
	c_R707();
	c_R707* m_new();
	void mark();
};
class c_R708 : public c_FieldInfo{
	public:
	c_R708();
	c_R708* m_new();
	void mark();
};
class c_R709 : public c_FieldInfo{
	public:
	c_R709();
	c_R709* m_new();
	void mark();
};
class c_R710 : public c_MethodInfo{
	public:
	c_R710();
	c_R710* m_new();
	void mark();
};
class c_R711 : public c_MethodInfo{
	public:
	c_R711();
	c_R711* m_new();
	void mark();
};
class c_R712 : public c_MethodInfo{
	public:
	c_R712();
	c_R712* m_new();
	void mark();
};
class c_R713 : public c_MethodInfo{
	public:
	c_R713();
	c_R713* m_new();
	void mark();
};
class c_R714 : public c_MethodInfo{
	public:
	c_R714();
	c_R714* m_new();
	void mark();
};
class c_R715 : public c_MethodInfo{
	public:
	c_R715();
	c_R715* m_new();
	void mark();
};
class c_R716 : public c_MethodInfo{
	public:
	c_R716();
	c_R716* m_new();
	void mark();
};
class c_R717 : public c_MethodInfo{
	public:
	c_R717();
	c_R717* m_new();
	void mark();
};
class c_R718 : public c_MethodInfo{
	public:
	c_R718();
	c_R718* m_new();
	void mark();
};
class c_R719 : public c_MethodInfo{
	public:
	c_R719();
	c_R719* m_new();
	void mark();
};
class c_R720 : public c_MethodInfo{
	public:
	c_R720();
	c_R720* m_new();
	void mark();
};
class c_R721 : public c_FunctionInfo{
	public:
	c_R721();
	c_R721* m_new();
	void mark();
};
class c_R746 : public c_FieldInfo{
	public:
	c_R746();
	c_R746* m_new();
	void mark();
};
class c_R723 : public c_MethodInfo{
	public:
	c_R723();
	c_R723* m_new();
	void mark();
};
class c_R724 : public c_MethodInfo{
	public:
	c_R724();
	c_R724* m_new();
	void mark();
};
class c_R725 : public c_MethodInfo{
	public:
	c_R725();
	c_R725* m_new();
	void mark();
};
class c_R726 : public c_MethodInfo{
	public:
	c_R726();
	c_R726* m_new();
	void mark();
};
class c_R727 : public c_MethodInfo{
	public:
	c_R727();
	c_R727* m_new();
	void mark();
};
class c_R728 : public c_MethodInfo{
	public:
	c_R728();
	c_R728* m_new();
	void mark();
};
class c_R729 : public c_MethodInfo{
	public:
	c_R729();
	c_R729* m_new();
	void mark();
};
class c_R730 : public c_MethodInfo{
	public:
	c_R730();
	c_R730* m_new();
	void mark();
};
class c_R731 : public c_MethodInfo{
	public:
	c_R731();
	c_R731* m_new();
	void mark();
};
class c_R732 : public c_MethodInfo{
	public:
	c_R732();
	c_R732* m_new();
	void mark();
};
class c_R733 : public c_MethodInfo{
	public:
	c_R733();
	c_R733* m_new();
	void mark();
};
class c_R734 : public c_MethodInfo{
	public:
	c_R734();
	c_R734* m_new();
	void mark();
};
class c_R735 : public c_MethodInfo{
	public:
	c_R735();
	c_R735* m_new();
	void mark();
};
class c_R736 : public c_MethodInfo{
	public:
	c_R736();
	c_R736* m_new();
	void mark();
};
class c_R737 : public c_MethodInfo{
	public:
	c_R737();
	c_R737* m_new();
	void mark();
};
class c_R738 : public c_MethodInfo{
	public:
	c_R738();
	c_R738* m_new();
	void mark();
};
class c_R739 : public c_MethodInfo{
	public:
	c_R739();
	c_R739* m_new();
	void mark();
};
class c_R740 : public c_MethodInfo{
	public:
	c_R740();
	c_R740* m_new();
	void mark();
};
class c_R741 : public c_MethodInfo{
	public:
	c_R741();
	c_R741* m_new();
	void mark();
};
class c_R742 : public c_MethodInfo{
	public:
	c_R742();
	c_R742* m_new();
	void mark();
};
class c_R743 : public c_MethodInfo{
	public:
	c_R743();
	c_R743* m_new();
	void mark();
};
class c_R744 : public c_MethodInfo{
	public:
	c_R744();
	c_R744* m_new();
	void mark();
};
class c_R745 : public c_MethodInfo{
	public:
	c_R745();
	c_R745* m_new();
	void mark();
};
class c_R747 : public c_FunctionInfo{
	public:
	c_R747();
	c_R747* m_new();
	void mark();
};
class c_R749 : public c_MethodInfo{
	public:
	c_R749();
	c_R749* m_new();
	void mark();
};
class c_R750 : public c_FunctionInfo{
	public:
	c_R750();
	c_R750* m_new();
	void mark();
};
class c_R759 : public c_FieldInfo{
	public:
	c_R759();
	c_R759* m_new();
	void mark();
};
class c_R760 : public c_FieldInfo{
	public:
	c_R760();
	c_R760* m_new();
	void mark();
};
class c_R761 : public c_FieldInfo{
	public:
	c_R761();
	c_R761* m_new();
	void mark();
};
class c_R762 : public c_FieldInfo{
	public:
	c_R762();
	c_R762* m_new();
	void mark();
};
class c_R763 : public c_FieldInfo{
	public:
	c_R763();
	c_R763* m_new();
	void mark();
};
class c_R764 : public c_FieldInfo{
	public:
	c_R764();
	c_R764* m_new();
	void mark();
};
class c_R753 : public c_MethodInfo{
	public:
	c_R753();
	c_R753* m_new();
	void mark();
};
class c_R754 : public c_MethodInfo{
	public:
	c_R754();
	c_R754* m_new();
	void mark();
};
class c_R755 : public c_MethodInfo{
	public:
	c_R755();
	c_R755* m_new();
	void mark();
};
class c_R756 : public c_MethodInfo{
	public:
	c_R756();
	c_R756* m_new();
	void mark();
};
class c_R757 : public c_MethodInfo{
	public:
	c_R757();
	c_R757* m_new();
	void mark();
};
class c_R758 : public c_MethodInfo{
	public:
	c_R758();
	c_R758* m_new();
	void mark();
};
class c_R752 : public c_FunctionInfo{
	public:
	c_R752();
	c_R752* m_new();
	void mark();
};
class c_R765 : public c_FunctionInfo{
	public:
	c_R765();
	c_R765* m_new();
	void mark();
};
class c_R769 : public c_FieldInfo{
	public:
	c_R769();
	c_R769* m_new();
	void mark();
};
class c_R768 : public c_MethodInfo{
	public:
	c_R768();
	c_R768* m_new();
	void mark();
};
class c_R767 : public c_FunctionInfo{
	public:
	c_R767();
	c_R767* m_new();
	void mark();
};
class c_R770 : public c_FunctionInfo{
	public:
	c_R770();
	c_R770* m_new();
	void mark();
};
class c_R775 : public c_FieldInfo{
	public:
	c_R775();
	c_R775* m_new();
	void mark();
};
class c_R773 : public c_MethodInfo{
	public:
	c_R773();
	c_R773* m_new();
	void mark();
};
class c_R774 : public c_MethodInfo{
	public:
	c_R774();
	c_R774* m_new();
	void mark();
};
class c_R772 : public c_FunctionInfo{
	public:
	c_R772();
	c_R772* m_new();
	void mark();
};
class c_R776 : public c_FunctionInfo{
	public:
	c_R776();
	c_R776* m_new();
	void mark();
};
class c_R778 : public c_FieldInfo{
	public:
	c_R778();
	c_R778* m_new();
	void mark();
};
class c_R779 : public c_FieldInfo{
	public:
	c_R779();
	c_R779* m_new();
	void mark();
};
class c_R780 : public c_MethodInfo{
	public:
	c_R780();
	c_R780* m_new();
	void mark();
};
class c_R781 : public c_MethodInfo{
	public:
	c_R781();
	c_R781* m_new();
	void mark();
};
class c_R782 : public c_MethodInfo{
	public:
	c_R782();
	c_R782* m_new();
	void mark();
};
class c_R783 : public c_MethodInfo{
	public:
	c_R783();
	c_R783* m_new();
	void mark();
};
class c_R784 : public c_MethodInfo{
	public:
	c_R784();
	c_R784* m_new();
	void mark();
};
class c_R785 : public c_MethodInfo{
	public:
	c_R785();
	c_R785* m_new();
	void mark();
};
class c_R786 : public c_MethodInfo{
	public:
	c_R786();
	c_R786* m_new();
	void mark();
};
class c_R787 : public c_MethodInfo{
	public:
	c_R787();
	c_R787* m_new();
	void mark();
};
class c_R788 : public c_FunctionInfo{
	public:
	c_R788();
	c_R788* m_new();
	void mark();
};
class c_R790 : public c_FieldInfo{
	public:
	c_R790();
	c_R790* m_new();
	void mark();
};
class c_R793 : public c_MethodInfo{
	public:
	c_R793();
	c_R793* m_new();
	void mark();
};
class c_R794 : public c_MethodInfo{
	public:
	c_R794();
	c_R794* m_new();
	void mark();
};
class c_R795 : public c_MethodInfo{
	public:
	c_R795();
	c_R795* m_new();
	void mark();
};
class c_R796 : public c_MethodInfo{
	public:
	c_R796();
	c_R796* m_new();
	void mark();
};
class c_R797 : public c_MethodInfo{
	public:
	c_R797();
	c_R797* m_new();
	void mark();
};
class c_R798 : public c_MethodInfo{
	public:
	c_R798();
	c_R798* m_new();
	void mark();
};
class c_R799 : public c_MethodInfo{
	public:
	c_R799();
	c_R799* m_new();
	void mark();
};
class c_R800 : public c_MethodInfo{
	public:
	c_R800();
	c_R800* m_new();
	void mark();
};
class c_R801 : public c_MethodInfo{
	public:
	c_R801();
	c_R801* m_new();
	void mark();
};
class c_R802 : public c_MethodInfo{
	public:
	c_R802();
	c_R802* m_new();
	void mark();
};
class c_R803 : public c_MethodInfo{
	public:
	c_R803();
	c_R803* m_new();
	void mark();
};
class c_R804 : public c_MethodInfo{
	public:
	c_R804();
	c_R804* m_new();
	void mark();
};
class c_R805 : public c_MethodInfo{
	public:
	c_R805();
	c_R805* m_new();
	void mark();
};
class c_R806 : public c_MethodInfo{
	public:
	c_R806();
	c_R806* m_new();
	void mark();
};
class c_R807 : public c_MethodInfo{
	public:
	c_R807();
	c_R807* m_new();
	void mark();
};
class c_R791 : public c_FunctionInfo{
	public:
	c_R791();
	c_R791* m_new();
	void mark();
};
class c_R792 : public c_FunctionInfo{
	public:
	c_R792();
	c_R792* m_new();
	void mark();
};
class c_R808 : public c_FunctionInfo{
	public:
	c_R808();
	c_R808* m_new();
	void mark();
};
class c_R810 : public c_FieldInfo{
	public:
	c_R810();
	c_R810* m_new();
	void mark();
};
class c_R812 : public c_MethodInfo{
	public:
	c_R812();
	c_R812* m_new();
	void mark();
};
class c_R813 : public c_MethodInfo{
	public:
	c_R813();
	c_R813* m_new();
	void mark();
};
class c_R814 : public c_MethodInfo{
	public:
	c_R814();
	c_R814* m_new();
	void mark();
};
class c_R815 : public c_MethodInfo{
	public:
	c_R815();
	c_R815* m_new();
	void mark();
};
class c_R816 : public c_MethodInfo{
	public:
	c_R816();
	c_R816* m_new();
	void mark();
};
class c_R817 : public c_MethodInfo{
	public:
	c_R817();
	c_R817* m_new();
	void mark();
};
class c_R818 : public c_MethodInfo{
	public:
	c_R818();
	c_R818* m_new();
	void mark();
};
class c_R819 : public c_MethodInfo{
	public:
	c_R819();
	c_R819* m_new();
	void mark();
};
class c_R811 : public c_FunctionInfo{
	public:
	c_R811();
	c_R811* m_new();
	void mark();
};
class c_R820 : public c_FunctionInfo{
	public:
	c_R820();
	c_R820* m_new();
	void mark();
};
class c_R822 : public c_FieldInfo{
	public:
	c_R822();
	c_R822* m_new();
	void mark();
};
class c_R823 : public c_FieldInfo{
	public:
	c_R823();
	c_R823* m_new();
	void mark();
};
class c_R824 : public c_FieldInfo{
	public:
	c_R824();
	c_R824* m_new();
	void mark();
};
class c_R825 : public c_FieldInfo{
	public:
	c_R825();
	c_R825* m_new();
	void mark();
};
class c_R841 : public c_FieldInfo{
	public:
	c_R841();
	c_R841* m_new();
	void mark();
};
class c_R827 : public c_MethodInfo{
	public:
	c_R827();
	c_R827* m_new();
	void mark();
};
class c_R828 : public c_MethodInfo{
	public:
	c_R828();
	c_R828* m_new();
	void mark();
};
class c_R829 : public c_MethodInfo{
	public:
	c_R829();
	c_R829* m_new();
	void mark();
};
class c_R830 : public c_MethodInfo{
	public:
	c_R830();
	c_R830* m_new();
	void mark();
};
class c_R831 : public c_MethodInfo{
	public:
	c_R831();
	c_R831* m_new();
	void mark();
};
class c_R832 : public c_MethodInfo{
	public:
	c_R832();
	c_R832* m_new();
	void mark();
};
class c_R833 : public c_MethodInfo{
	public:
	c_R833();
	c_R833* m_new();
	void mark();
};
class c_R834 : public c_MethodInfo{
	public:
	c_R834();
	c_R834* m_new();
	void mark();
};
class c_R835 : public c_MethodInfo{
	public:
	c_R835();
	c_R835* m_new();
	void mark();
};
class c_R836 : public c_MethodInfo{
	public:
	c_R836();
	c_R836* m_new();
	void mark();
};
class c_R837 : public c_MethodInfo{
	public:
	c_R837();
	c_R837* m_new();
	void mark();
};
class c_R838 : public c_MethodInfo{
	public:
	c_R838();
	c_R838* m_new();
	void mark();
};
class c_R839 : public c_MethodInfo{
	public:
	c_R839();
	c_R839* m_new();
	void mark();
};
class c_R840 : public c_MethodInfo{
	public:
	c_R840();
	c_R840* m_new();
	void mark();
};
class c_R826 : public c_FunctionInfo{
	public:
	c_R826();
	c_R826* m_new();
	void mark();
};
class c_R842 : public c_FunctionInfo{
	public:
	c_R842();
	c_R842* m_new();
	void mark();
};
class c_R847 : public c_FieldInfo{
	public:
	c_R847();
	c_R847* m_new();
	void mark();
};
class c_R848 : public c_FieldInfo{
	public:
	c_R848();
	c_R848* m_new();
	void mark();
};
class c_R845 : public c_MethodInfo{
	public:
	c_R845();
	c_R845* m_new();
	void mark();
};
class c_R846 : public c_MethodInfo{
	public:
	c_R846();
	c_R846* m_new();
	void mark();
};
class c_R844 : public c_FunctionInfo{
	public:
	c_R844();
	c_R844* m_new();
	void mark();
};
class c_R849 : public c_FunctionInfo{
	public:
	c_R849();
	c_R849* m_new();
	void mark();
};
class c_R854 : public c_FieldInfo{
	public:
	c_R854();
	c_R854* m_new();
	void mark();
};
class c_R855 : public c_FieldInfo{
	public:
	c_R855();
	c_R855* m_new();
	void mark();
};
class c_R852 : public c_MethodInfo{
	public:
	c_R852();
	c_R852* m_new();
	void mark();
};
class c_R853 : public c_MethodInfo{
	public:
	c_R853();
	c_R853* m_new();
	void mark();
};
class c_R851 : public c_FunctionInfo{
	public:
	c_R851();
	c_R851* m_new();
	void mark();
};
class c_R856 : public c_FunctionInfo{
	public:
	c_R856();
	c_R856* m_new();
	void mark();
};
class c_R861 : public c_FieldInfo{
	public:
	c_R861();
	c_R861* m_new();
	void mark();
};
class c_R862 : public c_FieldInfo{
	public:
	c_R862();
	c_R862* m_new();
	void mark();
};
class c_R859 : public c_MethodInfo{
	public:
	c_R859();
	c_R859* m_new();
	void mark();
};
class c_R860 : public c_MethodInfo{
	public:
	c_R860();
	c_R860* m_new();
	void mark();
};
class c_R858 : public c_FunctionInfo{
	public:
	c_R858();
	c_R858* m_new();
	void mark();
};
class c_R863 : public c_FunctionInfo{
	public:
	c_R863();
	c_R863* m_new();
	void mark();
};
class c_R868 : public c_FieldInfo{
	public:
	c_R868();
	c_R868* m_new();
	void mark();
};
class c_R869 : public c_FieldInfo{
	public:
	c_R869();
	c_R869* m_new();
	void mark();
};
class c_R866 : public c_MethodInfo{
	public:
	c_R866();
	c_R866* m_new();
	void mark();
};
class c_R867 : public c_MethodInfo{
	public:
	c_R867();
	c_R867* m_new();
	void mark();
};
class c_R865 : public c_FunctionInfo{
	public:
	c_R865();
	c_R865* m_new();
	void mark();
};
class c_R870 : public c_FunctionInfo{
	public:
	c_R870();
	c_R870* m_new();
	void mark();
};
class c_R874 : public c_FieldInfo{
	public:
	c_R874();
	c_R874* m_new();
	void mark();
};
class c_R873 : public c_MethodInfo{
	public:
	c_R873();
	c_R873* m_new();
	void mark();
};
class c_R872 : public c_FunctionInfo{
	public:
	c_R872();
	c_R872* m_new();
	void mark();
};
class c_R875 : public c_FunctionInfo{
	public:
	c_R875();
	c_R875* m_new();
	void mark();
};
class c_R880 : public c_FieldInfo{
	public:
	c_R880();
	c_R880* m_new();
	void mark();
};
class c_R881 : public c_FieldInfo{
	public:
	c_R881();
	c_R881* m_new();
	void mark();
};
class c_R878 : public c_MethodInfo{
	public:
	c_R878();
	c_R878* m_new();
	void mark();
};
class c_R879 : public c_MethodInfo{
	public:
	c_R879();
	c_R879* m_new();
	void mark();
};
class c_R877 : public c_FunctionInfo{
	public:
	c_R877();
	c_R877* m_new();
	void mark();
};
class c_R882 : public c_FunctionInfo{
	public:
	c_R882();
	c_R882* m_new();
	void mark();
};
class c_R886 : public c_FieldInfo{
	public:
	c_R886();
	c_R886* m_new();
	void mark();
};
class c_R885 : public c_MethodInfo{
	public:
	c_R885();
	c_R885* m_new();
	void mark();
};
class c_R884 : public c_FunctionInfo{
	public:
	c_R884();
	c_R884* m_new();
	void mark();
};
class c_R887 : public c_FunctionInfo{
	public:
	c_R887();
	c_R887* m_new();
	void mark();
};
class c_R891 : public c_FieldInfo{
	public:
	c_R891();
	c_R891* m_new();
	void mark();
};
class c_R890 : public c_MethodInfo{
	public:
	c_R890();
	c_R890* m_new();
	void mark();
};
class c_R889 : public c_FunctionInfo{
	public:
	c_R889();
	c_R889* m_new();
	void mark();
};
class c_R892 : public c_FunctionInfo{
	public:
	c_R892();
	c_R892* m_new();
	void mark();
};
class c_R897 : public c_FieldInfo{
	public:
	c_R897();
	c_R897* m_new();
	void mark();
};
class c_R898 : public c_FieldInfo{
	public:
	c_R898();
	c_R898* m_new();
	void mark();
};
class c_R895 : public c_MethodInfo{
	public:
	c_R895();
	c_R895* m_new();
	void mark();
};
class c_R896 : public c_MethodInfo{
	public:
	c_R896();
	c_R896* m_new();
	void mark();
};
class c_R894 : public c_FunctionInfo{
	public:
	c_R894();
	c_R894* m_new();
	void mark();
};
class c_R899 : public c_FunctionInfo{
	public:
	c_R899();
	c_R899* m_new();
	void mark();
};
class c_R904 : public c_FieldInfo{
	public:
	c_R904();
	c_R904* m_new();
	void mark();
};
class c_R905 : public c_FieldInfo{
	public:
	c_R905();
	c_R905* m_new();
	void mark();
};
class c_R902 : public c_MethodInfo{
	public:
	c_R902();
	c_R902* m_new();
	void mark();
};
class c_R903 : public c_MethodInfo{
	public:
	c_R903();
	c_R903* m_new();
	void mark();
};
class c_R901 : public c_FunctionInfo{
	public:
	c_R901();
	c_R901* m_new();
	void mark();
};
class c_R906 : public c_FunctionInfo{
	public:
	c_R906();
	c_R906* m_new();
	void mark();
};
class c_R911 : public c_FieldInfo{
	public:
	c_R911();
	c_R911* m_new();
	void mark();
};
class c_R912 : public c_FieldInfo{
	public:
	c_R912();
	c_R912* m_new();
	void mark();
};
class c_R909 : public c_MethodInfo{
	public:
	c_R909();
	c_R909* m_new();
	void mark();
};
class c_R910 : public c_MethodInfo{
	public:
	c_R910();
	c_R910* m_new();
	void mark();
};
class c_R908 : public c_FunctionInfo{
	public:
	c_R908();
	c_R908* m_new();
	void mark();
};
class c_R913 : public c_FunctionInfo{
	public:
	c_R913();
	c_R913* m_new();
	void mark();
};
class c_R922 : public c_FieldInfo{
	public:
	c_R922();
	c_R922* m_new();
	void mark();
};
class c_R923 : public c_FieldInfo{
	public:
	c_R923();
	c_R923* m_new();
	void mark();
};
class c_R924 : public c_FieldInfo{
	public:
	c_R924();
	c_R924* m_new();
	void mark();
};
class c_R925 : public c_FieldInfo{
	public:
	c_R925();
	c_R925* m_new();
	void mark();
};
class c_R926 : public c_FieldInfo{
	public:
	c_R926();
	c_R926* m_new();
	void mark();
};
class c_R927 : public c_FieldInfo{
	public:
	c_R927();
	c_R927* m_new();
	void mark();
};
class c_R916 : public c_MethodInfo{
	public:
	c_R916();
	c_R916* m_new();
	void mark();
};
class c_R917 : public c_MethodInfo{
	public:
	c_R917();
	c_R917* m_new();
	void mark();
};
class c_R918 : public c_MethodInfo{
	public:
	c_R918();
	c_R918* m_new();
	void mark();
};
class c_R919 : public c_MethodInfo{
	public:
	c_R919();
	c_R919* m_new();
	void mark();
};
class c_R920 : public c_MethodInfo{
	public:
	c_R920();
	c_R920* m_new();
	void mark();
};
class c_R921 : public c_MethodInfo{
	public:
	c_R921();
	c_R921* m_new();
	void mark();
};
class c_R915 : public c_FunctionInfo{
	public:
	c_R915();
	c_R915* m_new();
	void mark();
};
class c_R928 : public c_FunctionInfo{
	public:
	c_R928();
	c_R928* m_new();
	void mark();
};
class c_R932 : public c_FieldInfo{
	public:
	c_R932();
	c_R932* m_new();
	void mark();
};
class c_R931 : public c_MethodInfo{
	public:
	c_R931();
	c_R931* m_new();
	void mark();
};
class c_R930 : public c_FunctionInfo{
	public:
	c_R930();
	c_R930* m_new();
	void mark();
};
class c_R933 : public c_FunctionInfo{
	public:
	c_R933();
	c_R933* m_new();
	void mark();
};
class c_R937 : public c_FieldInfo{
	public:
	c_R937();
	c_R937* m_new();
	void mark();
};
class c_R936 : public c_MethodInfo{
	public:
	c_R936();
	c_R936* m_new();
	void mark();
};
class c_R935 : public c_FunctionInfo{
	public:
	c_R935();
	c_R935* m_new();
	void mark();
};
class c_R938 : public c_FunctionInfo{
	public:
	c_R938();
	c_R938* m_new();
	void mark();
};
class c_R943 : public c_FieldInfo{
	public:
	c_R943();
	c_R943* m_new();
	void mark();
};
class c_R941 : public c_MethodInfo{
	public:
	c_R941();
	c_R941* m_new();
	void mark();
};
class c_R942 : public c_MethodInfo{
	public:
	c_R942();
	c_R942* m_new();
	void mark();
};
class c_R940 : public c_FunctionInfo{
	public:
	c_R940();
	c_R940* m_new();
	void mark();
};
class c_R944 : public c_FunctionInfo{
	public:
	c_R944();
	c_R944* m_new();
	void mark();
};
class c_R953 : public c_FieldInfo{
	public:
	c_R953();
	c_R953* m_new();
	void mark();
};
class c_R954 : public c_FieldInfo{
	public:
	c_R954();
	c_R954* m_new();
	void mark();
};
class c_R955 : public c_FieldInfo{
	public:
	c_R955();
	c_R955* m_new();
	void mark();
};
class c_R956 : public c_FieldInfo{
	public:
	c_R956();
	c_R956* m_new();
	void mark();
};
class c_R957 : public c_FieldInfo{
	public:
	c_R957();
	c_R957* m_new();
	void mark();
};
class c_R958 : public c_FieldInfo{
	public:
	c_R958();
	c_R958* m_new();
	void mark();
};
class c_R947 : public c_MethodInfo{
	public:
	c_R947();
	c_R947* m_new();
	void mark();
};
class c_R948 : public c_MethodInfo{
	public:
	c_R948();
	c_R948* m_new();
	void mark();
};
class c_R949 : public c_MethodInfo{
	public:
	c_R949();
	c_R949* m_new();
	void mark();
};
class c_R950 : public c_MethodInfo{
	public:
	c_R950();
	c_R950* m_new();
	void mark();
};
class c_R951 : public c_MethodInfo{
	public:
	c_R951();
	c_R951* m_new();
	void mark();
};
class c_R952 : public c_MethodInfo{
	public:
	c_R952();
	c_R952* m_new();
	void mark();
};
class c_R946 : public c_FunctionInfo{
	public:
	c_R946();
	c_R946* m_new();
	void mark();
};
class c_R959 : public c_FunctionInfo{
	public:
	c_R959();
	c_R959* m_new();
	void mark();
};
class c_R963 : public c_FieldInfo{
	public:
	c_R963();
	c_R963* m_new();
	void mark();
};
class c_R962 : public c_MethodInfo{
	public:
	c_R962();
	c_R962* m_new();
	void mark();
};
class c_R961 : public c_FunctionInfo{
	public:
	c_R961();
	c_R961* m_new();
	void mark();
};
class c_R964 : public c_FunctionInfo{
	public:
	c_R964();
	c_R964* m_new();
	void mark();
};
class c_R968 : public c_FieldInfo{
	public:
	c_R968();
	c_R968* m_new();
	void mark();
};
class c_R967 : public c_MethodInfo{
	public:
	c_R967();
	c_R967* m_new();
	void mark();
};
class c_R966 : public c_FunctionInfo{
	public:
	c_R966();
	c_R966* m_new();
	void mark();
};
class c_R969 : public c_FunctionInfo{
	public:
	c_R969();
	c_R969* m_new();
	void mark();
};
class c_R974 : public c_FieldInfo{
	public:
	c_R974();
	c_R974* m_new();
	void mark();
};
class c_R972 : public c_MethodInfo{
	public:
	c_R972();
	c_R972* m_new();
	void mark();
};
class c_R973 : public c_MethodInfo{
	public:
	c_R973();
	c_R973* m_new();
	void mark();
};
class c_R971 : public c_FunctionInfo{
	public:
	c_R971();
	c_R971* m_new();
	void mark();
};
class c_R975 : public c_FunctionInfo{
	public:
	c_R975();
	c_R975* m_new();
	void mark();
};
class c_R984 : public c_FieldInfo{
	public:
	c_R984();
	c_R984* m_new();
	void mark();
};
class c_R985 : public c_FieldInfo{
	public:
	c_R985();
	c_R985* m_new();
	void mark();
};
class c_R986 : public c_FieldInfo{
	public:
	c_R986();
	c_R986* m_new();
	void mark();
};
class c_R987 : public c_FieldInfo{
	public:
	c_R987();
	c_R987* m_new();
	void mark();
};
class c_R988 : public c_FieldInfo{
	public:
	c_R988();
	c_R988* m_new();
	void mark();
};
class c_R989 : public c_FieldInfo{
	public:
	c_R989();
	c_R989* m_new();
	void mark();
};
class c_R978 : public c_MethodInfo{
	public:
	c_R978();
	c_R978* m_new();
	void mark();
};
class c_R979 : public c_MethodInfo{
	public:
	c_R979();
	c_R979* m_new();
	void mark();
};
class c_R980 : public c_MethodInfo{
	public:
	c_R980();
	c_R980* m_new();
	void mark();
};
class c_R981 : public c_MethodInfo{
	public:
	c_R981();
	c_R981* m_new();
	void mark();
};
class c_R982 : public c_MethodInfo{
	public:
	c_R982();
	c_R982* m_new();
	void mark();
};
class c_R983 : public c_MethodInfo{
	public:
	c_R983();
	c_R983* m_new();
	void mark();
};
class c_R977 : public c_FunctionInfo{
	public:
	c_R977();
	c_R977* m_new();
	void mark();
};
class c_R990 : public c_FunctionInfo{
	public:
	c_R990();
	c_R990* m_new();
	void mark();
};
class c_R994 : public c_FieldInfo{
	public:
	c_R994();
	c_R994* m_new();
	void mark();
};
class c_R993 : public c_MethodInfo{
	public:
	c_R993();
	c_R993* m_new();
	void mark();
};
class c_R992 : public c_FunctionInfo{
	public:
	c_R992();
	c_R992* m_new();
	void mark();
};
class c_R995 : public c_FunctionInfo{
	public:
	c_R995();
	c_R995* m_new();
	void mark();
};
class c_R999 : public c_FieldInfo{
	public:
	c_R999();
	c_R999* m_new();
	void mark();
};
class c_R998 : public c_MethodInfo{
	public:
	c_R998();
	c_R998* m_new();
	void mark();
};
class c_R997 : public c_FunctionInfo{
	public:
	c_R997();
	c_R997* m_new();
	void mark();
};
class c_R1000 : public c_FunctionInfo{
	public:
	c_R1000();
	c_R1000* m_new();
	void mark();
};
class c_R1005 : public c_FieldInfo{
	public:
	c_R1005();
	c_R1005* m_new();
	void mark();
};
class c_R1003 : public c_MethodInfo{
	public:
	c_R1003();
	c_R1003* m_new();
	void mark();
};
class c_R1004 : public c_MethodInfo{
	public:
	c_R1004();
	c_R1004* m_new();
	void mark();
};
class c_R1002 : public c_FunctionInfo{
	public:
	c_R1002();
	c_R1002* m_new();
	void mark();
};
class c_R1006 : public c_FunctionInfo{
	public:
	c_R1006();
	c_R1006* m_new();
	void mark();
};
class c_R1010 : public c_FieldInfo{
	public:
	c_R1010();
	c_R1010* m_new();
	void mark();
};
class c_R1009 : public c_MethodInfo{
	public:
	c_R1009();
	c_R1009* m_new();
	void mark();
};
class c_R1008 : public c_FunctionInfo{
	public:
	c_R1008();
	c_R1008* m_new();
	void mark();
};
class c_R1011 : public c_FunctionInfo{
	public:
	c_R1011();
	c_R1011* m_new();
	void mark();
};
class c_R1016 : public c_FieldInfo{
	public:
	c_R1016();
	c_R1016* m_new();
	void mark();
};
class c_R1014 : public c_MethodInfo{
	public:
	c_R1014();
	c_R1014* m_new();
	void mark();
};
class c_R1015 : public c_MethodInfo{
	public:
	c_R1015();
	c_R1015* m_new();
	void mark();
};
class c_R1013 : public c_FunctionInfo{
	public:
	c_R1013();
	c_R1013* m_new();
	void mark();
};
class c_R1017 : public c_FunctionInfo{
	public:
	c_R1017();
	c_R1017* m_new();
	void mark();
};
class c_R1022 : public c_FieldInfo{
	public:
	c_R1022();
	c_R1022* m_new();
	void mark();
};
class c_R1020 : public c_MethodInfo{
	public:
	c_R1020();
	c_R1020* m_new();
	void mark();
};
class c_R1021 : public c_MethodInfo{
	public:
	c_R1021();
	c_R1021* m_new();
	void mark();
};
class c_R1019 : public c_FunctionInfo{
	public:
	c_R1019();
	c_R1019* m_new();
	void mark();
};
class c_R1023 : public c_FunctionInfo{
	public:
	c_R1023();
	c_R1023* m_new();
	void mark();
};
class c_R1028 : public c_FieldInfo{
	public:
	c_R1028();
	c_R1028* m_new();
	void mark();
};
class c_R1026 : public c_MethodInfo{
	public:
	c_R1026();
	c_R1026* m_new();
	void mark();
};
class c_R1027 : public c_MethodInfo{
	public:
	c_R1027();
	c_R1027* m_new();
	void mark();
};
class c_R1025 : public c_FunctionInfo{
	public:
	c_R1025();
	c_R1025* m_new();
	void mark();
};
class c_R1029 : public c_FunctionInfo{
	public:
	c_R1029();
	c_R1029* m_new();
	void mark();
};
class c_R1034 : public c_FieldInfo{
	public:
	c_R1034();
	c_R1034* m_new();
	void mark();
};
class c_R1032 : public c_MethodInfo{
	public:
	c_R1032();
	c_R1032* m_new();
	void mark();
};
class c_R1033 : public c_MethodInfo{
	public:
	c_R1033();
	c_R1033* m_new();
	void mark();
};
class c_R1031 : public c_FunctionInfo{
	public:
	c_R1031();
	c_R1031* m_new();
	void mark();
};
class c_R1035 : public c_FunctionInfo{
	public:
	c_R1035();
	c_R1035* m_new();
	void mark();
};
class c_R1040 : public c_FieldInfo{
	public:
	c_R1040();
	c_R1040* m_new();
	void mark();
};
class c_R1038 : public c_MethodInfo{
	public:
	c_R1038();
	c_R1038* m_new();
	void mark();
};
class c_R1039 : public c_MethodInfo{
	public:
	c_R1039();
	c_R1039* m_new();
	void mark();
};
class c_R1037 : public c_FunctionInfo{
	public:
	c_R1037();
	c_R1037* m_new();
	void mark();
};
class c_R1041 : public c_FunctionInfo{
	public:
	c_R1041();
	c_R1041* m_new();
	void mark();
};
class c_R1046 : public c_FieldInfo{
	public:
	c_R1046();
	c_R1046* m_new();
	void mark();
};
class c_R1044 : public c_MethodInfo{
	public:
	c_R1044();
	c_R1044* m_new();
	void mark();
};
class c_R1045 : public c_MethodInfo{
	public:
	c_R1045();
	c_R1045* m_new();
	void mark();
};
class c_R1043 : public c_FunctionInfo{
	public:
	c_R1043();
	c_R1043* m_new();
	void mark();
};
class c_R1047 : public c_FunctionInfo{
	public:
	c_R1047();
	c_R1047* m_new();
	void mark();
};
class c_R1052 : public c_FieldInfo{
	public:
	c_R1052();
	c_R1052* m_new();
	void mark();
};
class c_R1050 : public c_MethodInfo{
	public:
	c_R1050();
	c_R1050* m_new();
	void mark();
};
class c_R1051 : public c_MethodInfo{
	public:
	c_R1051();
	c_R1051* m_new();
	void mark();
};
class c_R1049 : public c_FunctionInfo{
	public:
	c_R1049();
	c_R1049* m_new();
	void mark();
};
class c_R1053 : public c_FunctionInfo{
	public:
	c_R1053();
	c_R1053* m_new();
	void mark();
};
class c_R1058 : public c_FieldInfo{
	public:
	c_R1058();
	c_R1058* m_new();
	void mark();
};
class c_R1056 : public c_MethodInfo{
	public:
	c_R1056();
	c_R1056* m_new();
	void mark();
};
class c_R1057 : public c_MethodInfo{
	public:
	c_R1057();
	c_R1057* m_new();
	void mark();
};
class c_R1055 : public c_FunctionInfo{
	public:
	c_R1055();
	c_R1055* m_new();
	void mark();
};
class c_R1059 : public c_FunctionInfo{
	public:
	c_R1059();
	c_R1059* m_new();
	void mark();
};
class c_R1064 : public c_FieldInfo{
	public:
	c_R1064();
	c_R1064* m_new();
	void mark();
};
class c_R1065 : public c_FieldInfo{
	public:
	c_R1065();
	c_R1065* m_new();
	void mark();
};
class c_R1062 : public c_MethodInfo{
	public:
	c_R1062();
	c_R1062* m_new();
	void mark();
};
class c_R1063 : public c_MethodInfo{
	public:
	c_R1063();
	c_R1063* m_new();
	void mark();
};
class c_R1061 : public c_FunctionInfo{
	public:
	c_R1061();
	c_R1061* m_new();
	void mark();
};
class c_R1066 : public c_FunctionInfo{
	public:
	c_R1066();
	c_R1066* m_new();
	void mark();
};
class c_R1070 : public c_FieldInfo{
	public:
	c_R1070();
	c_R1070* m_new();
	void mark();
};
class c_R1069 : public c_MethodInfo{
	public:
	c_R1069();
	c_R1069* m_new();
	void mark();
};
class c_R1068 : public c_FunctionInfo{
	public:
	c_R1068();
	c_R1068* m_new();
	void mark();
};
class c_R1071 : public c_FunctionInfo{
	public:
	c_R1071();
	c_R1071* m_new();
	void mark();
};
class c_R1076 : public c_FieldInfo{
	public:
	c_R1076();
	c_R1076* m_new();
	void mark();
};
class c_R1077 : public c_FieldInfo{
	public:
	c_R1077();
	c_R1077* m_new();
	void mark();
};
class c_R1074 : public c_MethodInfo{
	public:
	c_R1074();
	c_R1074* m_new();
	void mark();
};
class c_R1075 : public c_MethodInfo{
	public:
	c_R1075();
	c_R1075* m_new();
	void mark();
};
class c_R1073 : public c_FunctionInfo{
	public:
	c_R1073();
	c_R1073* m_new();
	void mark();
};
class c_R1078 : public c_FunctionInfo{
	public:
	c_R1078();
	c_R1078* m_new();
	void mark();
};
class c_R1082 : public c_FieldInfo{
	public:
	c_R1082();
	c_R1082* m_new();
	void mark();
};
class c_R1081 : public c_MethodInfo{
	public:
	c_R1081();
	c_R1081* m_new();
	void mark();
};
class c_R1080 : public c_FunctionInfo{
	public:
	c_R1080();
	c_R1080* m_new();
	void mark();
};
class c_R1083 : public c_FunctionInfo{
	public:
	c_R1083();
	c_R1083* m_new();
	void mark();
};
class c_R1088 : public c_FieldInfo{
	public:
	c_R1088();
	c_R1088* m_new();
	void mark();
};
class c_R1089 : public c_FieldInfo{
	public:
	c_R1089();
	c_R1089* m_new();
	void mark();
};
class c_R1086 : public c_MethodInfo{
	public:
	c_R1086();
	c_R1086* m_new();
	void mark();
};
class c_R1087 : public c_MethodInfo{
	public:
	c_R1087();
	c_R1087* m_new();
	void mark();
};
class c_R1085 : public c_FunctionInfo{
	public:
	c_R1085();
	c_R1085* m_new();
	void mark();
};
class c_R1090 : public c_FunctionInfo{
	public:
	c_R1090();
	c_R1090* m_new();
	void mark();
};
class c_R1094 : public c_FieldInfo{
	public:
	c_R1094();
	c_R1094* m_new();
	void mark();
};
class c_R1093 : public c_MethodInfo{
	public:
	c_R1093();
	c_R1093* m_new();
	void mark();
};
class c_R1092 : public c_FunctionInfo{
	public:
	c_R1092();
	c_R1092* m_new();
	void mark();
};
class c_R1095 : public c_FunctionInfo{
	public:
	c_R1095();
	c_R1095* m_new();
	void mark();
};
class c_R1100 : public c_FieldInfo{
	public:
	c_R1100();
	c_R1100* m_new();
	void mark();
};
class c_R1101 : public c_FieldInfo{
	public:
	c_R1101();
	c_R1101* m_new();
	void mark();
};
class c_R1098 : public c_MethodInfo{
	public:
	c_R1098();
	c_R1098* m_new();
	void mark();
};
class c_R1099 : public c_MethodInfo{
	public:
	c_R1099();
	c_R1099* m_new();
	void mark();
};
class c_R1097 : public c_FunctionInfo{
	public:
	c_R1097();
	c_R1097* m_new();
	void mark();
};
class c_R1102 : public c_FunctionInfo{
	public:
	c_R1102();
	c_R1102* m_new();
	void mark();
};
class c_R1107 : public c_FieldInfo{
	public:
	c_R1107();
	c_R1107* m_new();
	void mark();
};
class c_R1108 : public c_FieldInfo{
	public:
	c_R1108();
	c_R1108* m_new();
	void mark();
};
class c_R1105 : public c_MethodInfo{
	public:
	c_R1105();
	c_R1105* m_new();
	void mark();
};
class c_R1106 : public c_MethodInfo{
	public:
	c_R1106();
	c_R1106* m_new();
	void mark();
};
class c_R1104 : public c_FunctionInfo{
	public:
	c_R1104();
	c_R1104* m_new();
	void mark();
};
class c_R1109 : public c_FunctionInfo{
	public:
	c_R1109();
	c_R1109* m_new();
	void mark();
};
class c_R1114 : public c_FieldInfo{
	public:
	c_R1114();
	c_R1114* m_new();
	void mark();
};
class c_R1115 : public c_FieldInfo{
	public:
	c_R1115();
	c_R1115* m_new();
	void mark();
};
class c_R1112 : public c_MethodInfo{
	public:
	c_R1112();
	c_R1112* m_new();
	void mark();
};
class c_R1113 : public c_MethodInfo{
	public:
	c_R1113();
	c_R1113* m_new();
	void mark();
};
class c_R1111 : public c_FunctionInfo{
	public:
	c_R1111();
	c_R1111* m_new();
	void mark();
};
class c_R1116 : public c_FunctionInfo{
	public:
	c_R1116();
	c_R1116* m_new();
	void mark();
};
class c_R1118 : public c_FieldInfo{
	public:
	c_R1118();
	c_R1118* m_new();
	void mark();
};
class c_R1120 : public c_MethodInfo{
	public:
	c_R1120();
	c_R1120* m_new();
	void mark();
};
class c_R1119 : public c_FunctionInfo{
	public:
	c_R1119();
	c_R1119* m_new();
	void mark();
};
class c_R1121 : public c_FunctionInfo{
	public:
	c_R1121();
	c_R1121* m_new();
	void mark();
};
class c_R1123 : public c_FieldInfo{
	public:
	c_R1123();
	c_R1123* m_new();
	void mark();
};
class c_R1125 : public c_MethodInfo{
	public:
	c_R1125();
	c_R1125* m_new();
	void mark();
};
class c_R1124 : public c_FunctionInfo{
	public:
	c_R1124();
	c_R1124* m_new();
	void mark();
};
class c_R1126 : public c_FunctionInfo{
	public:
	c_R1126();
	c_R1126* m_new();
	void mark();
};
class c_R1128 : public c_FieldInfo{
	public:
	c_R1128();
	c_R1128* m_new();
	void mark();
};
class c_R1130 : public c_MethodInfo{
	public:
	c_R1130();
	c_R1130* m_new();
	void mark();
};
class c_R1129 : public c_FunctionInfo{
	public:
	c_R1129();
	c_R1129* m_new();
	void mark();
};
class c_R1131 : public c_FunctionInfo{
	public:
	c_R1131();
	c_R1131* m_new();
	void mark();
};
int bb_graphics_SetGraphicsDevice(gxtkGraphics*);
int bb_graphics_SetFont(c_Image*,int);
extern gxtkAudio* bb_audio_device;
int bb_audio_SetAudioDevice(gxtkAudio*);
class c_InputDevice : public Object{
	public:
	Array<c_JoyState* > m__joyStates;
	Array<bool > m__keyDown;
	int m__keyHitPut;
	Array<int > m__keyHitQueue;
	Array<int > m__keyHit;
	int m__charGet;
	int m__charPut;
	Array<int > m__charQueue;
	Float m__mouseX;
	Float m__mouseY;
	Array<Float > m__touchX;
	Array<Float > m__touchY;
	Float m__accelX;
	Float m__accelY;
	Float m__accelZ;
	c_InputDevice();
	c_InputDevice* m_new();
	void p_PutKeyHit(int);
	void p_BeginUpdate();
	void p_EndUpdate();
	void p_KeyEvent(int,int);
	void p_MouseEvent(int,int,Float,Float);
	void p_TouchEvent(int,int,Float,Float);
	void p_MotionEvent(int,int,Float,Float,Float);
	int p_KeyHit(int);
	void mark();
};
class c_JoyState : public Object{
	public:
	Array<Float > m_joyx;
	Array<Float > m_joyy;
	Array<Float > m_joyz;
	Array<bool > m_buttons;
	c_JoyState();
	c_JoyState* m_new();
	void mark();
};
extern c_InputDevice* bb_input_device;
int bb_input_SetInputDevice(c_InputDevice*);
extern int bb_app__devWidth;
extern int bb_app__devHeight;
void bb_app_ValidateDeviceWindow(bool);
class c_DisplayMode : public Object{
	public:
	int m__width;
	int m__height;
	c_DisplayMode();
	c_DisplayMode* m_new(int,int);
	c_DisplayMode* m_new2();
	void mark();
};
class c_Map6 : public Object{
	public:
	c_Node9* m_root;
	c_Map6();
	c_Map6* m_new();
	virtual int p_Compare4(int,int)=0;
	c_Node9* p_FindNode(int);
	bool p_Contains(int);
	int p_RotateLeft7(c_Node9*);
	int p_RotateRight6(c_Node9*);
	int p_InsertFixup6(c_Node9*);
	bool p_Set14(int,c_DisplayMode*);
	bool p_Insert12(int,c_DisplayMode*);
	void mark();
};
class c_IntMap2 : public c_Map6{
	public:
	c_IntMap2();
	c_IntMap2* m_new();
	int p_Compare4(int,int);
	void mark();
};
class c_Stack9 : public Object{
	public:
	Array<c_DisplayMode* > m_data;
	int m_length;
	c_Stack9();
	c_Stack9* m_new();
	c_Stack9* m_new2(Array<c_DisplayMode* >);
	void p_Push25(c_DisplayMode*);
	void p_Push26(Array<c_DisplayMode* >,int,int);
	void p_Push27(Array<c_DisplayMode* >,int);
	Array<c_DisplayMode* > p_ToArray();
	void mark();
};
class c_Node9 : public Object{
	public:
	int m_key;
	c_Node9* m_right;
	c_Node9* m_left;
	c_DisplayMode* m_value;
	int m_color;
	c_Node9* m_parent;
	c_Node9();
	c_Node9* m_new(int,c_DisplayMode*,int,c_Node9*);
	c_Node9* m_new2();
	void mark();
};
extern Array<c_DisplayMode* > bb_app__displayModes;
extern c_DisplayMode* bb_app__desktopMode;
int bb_app_DeviceWidth();
int bb_app_DeviceHeight();
void bb_app_EnumDisplayModes();
int bb_graphics_SetBlend(int);
int bb_graphics_SetScissor(Float,Float,Float,Float);
int bb_graphics_BeginRender();
int bb_graphics_EndRender();
class c_BBGameEvent : public Object{
	public:
	c_BBGameEvent();
	void mark();
};
void bb_app_EndApp();
class c_VTransition : public Object{
	public:
	Float m_duration;
	c_Color* m_color;
	bool m_active;
	Float m_time;
	c_VTransition();
	void p_Duration(Float);
	Float p_Duration2();
	c_VTransition* m_new(Float);
	c_VTransition* m_new2();
	void p_SetColor2(c_Color*);
	void p_Update4(Float);
	bool p_IsActive();
	virtual void p_Render();
	void mark();
};
class c_VFadeInLinear : public c_VTransition{
	public:
	c_VFadeInLinear();
	c_VFadeInLinear* m_new();
	c_VFadeInLinear* m_new2(Float);
	void p_Render();
	void mark();
};
class c_List4 : public Object{
	public:
	c_Node10* m__head;
	c_List4();
	c_List4* m_new();
	c_Node10* p_AddLast4(c_VShape*);
	c_List4* m_new2(Array<c_VShape* >);
	c_VShape* p_First();
	c_Enumerator12* p_ObjectEnumerator();
	void mark();
};
class c_Node10 : public Object{
	public:
	c_Node10* m__succ;
	c_Node10* m__pred;
	c_VShape* m__data;
	c_Node10();
	c_Node10* m_new(c_Node10*,c_Node10*,c_VShape*);
	c_Node10* m_new2();
	void mark();
};
class c_HeadNode4 : public c_Node10{
	public:
	c_HeadNode4();
	c_HeadNode4* m_new();
	void mark();
};
extern int bb_app__updateRate;
void bb_app_SetUpdateRate(int);
class c_FontCache : public Object{
	public:
	c_FontCache();
	static c_StringMap4* m_Cache;
	static c_AngelFont* m_GetFont(String);
	void mark();
};
class c_AngelFont : public Object{
	public:
	String m_iniText;
	Array<c_Char* > m_chars;
	int m_height;
	int m_heightOffset;
	c_IntMap4* m_kernPairs;
	Array<c_Image* > m_image;
	String m_name;
	int m_xOffset;
	int m_yOffset;
	int m_lineGap;
	bool m_useKerning;
	c_AngelFont();
	static c_IntMap3* m_firstKp;
	void p_LoadPlain(String);
	static c_StringMap4* m__list;
	c_AngelFont* m_new(String);
	void p_LoadFromXml(String);
	static c_KernPair* m_secondKp;
	void p_DrawText(String,int,int);
	int p_TextWidth(String);
	int p_TextHeight(String);
	void p_DrawText2(String,int,int,int,int);
	void mark();
};
class c_Map7 : public Object{
	public:
	c_Node11* m_root;
	c_Map7();
	c_Map7* m_new();
	virtual int p_Compare6(String,String)=0;
	c_Node11* p_FindNode3(String);
	bool p_Contains3(String);
	c_AngelFont* p_Get3(String);
	int p_RotateLeft8(c_Node11*);
	int p_RotateRight7(c_Node11*);
	int p_InsertFixup7(c_Node11*);
	bool p_Set15(String,c_AngelFont*);
	bool p_Insert13(String,c_AngelFont*);
	void mark();
};
class c_StringMap4 : public c_Map7{
	public:
	c_StringMap4();
	c_StringMap4* m_new();
	int p_Compare6(String,String);
	void mark();
};
class c_Node11 : public Object{
	public:
	String m_key;
	c_Node11* m_right;
	c_Node11* m_left;
	c_AngelFont* m_value;
	int m_color;
	c_Node11* m_parent;
	c_Node11();
	c_Node11* m_new(String,c_AngelFont*,int,c_Node11*);
	c_Node11* m_new2();
	void mark();
};
String bb_app_LoadString(String);
class c_Char : public Object{
	public:
	int m_x;
	int m_y;
	int m_width;
	int m_height;
	int m_xOffset;
	int m_yOffset;
	int m_xAdvance;
	int m_page;
	c_Char();
	c_Char* m_new(int,int,int,int,int,int,int,int);
	c_Char* m_new2();
	void p_Draw2(c_Image*,int,int);
	void mark();
};
class c_KernPair : public Object{
	public:
	String m_first;
	String m_second;
	int m_amount;
	c_KernPair();
	c_KernPair* m_new(int,int,int);
	c_KernPair* m_new2();
	void mark();
};
class c_Map8 : public Object{
	public:
	c_Node13* m_root;
	c_Map8();
	c_Map8* m_new();
	virtual int p_Compare4(int,int)=0;
	int p_RotateLeft9(c_Node13*);
	int p_RotateRight8(c_Node13*);
	int p_InsertFixup8(c_Node13*);
	bool p_Add8(int,c_KernPair*);
	c_Node13* p_FindNode(int);
	c_KernPair* p_Get(int);
	void mark();
};
class c_IntMap3 : public c_Map8{
	public:
	c_IntMap3();
	c_IntMap3* m_new();
	int p_Compare4(int,int);
	void mark();
};
class c_Map9 : public Object{
	public:
	c_Node12* m_root;
	c_Map9();
	c_Map9* m_new();
	virtual int p_Compare4(int,int)=0;
	c_Node12* p_FindNode(int);
	c_IntMap3* p_Get(int);
	int p_RotateLeft10(c_Node12*);
	int p_RotateRight9(c_Node12*);
	int p_InsertFixup9(c_Node12*);
	bool p_Add9(int,c_IntMap3*);
	void mark();
};
class c_IntMap4 : public c_Map9{
	public:
	c_IntMap4();
	c_IntMap4* m_new();
	int p_Compare4(int,int);
	void mark();
};
class c_Node12 : public Object{
	public:
	int m_key;
	c_Node12* m_right;
	c_Node12* m_left;
	c_IntMap3* m_value;
	int m_color;
	c_Node12* m_parent;
	c_Node12();
	c_Node12* m_new(int,c_IntMap3*,int,c_Node12*);
	c_Node12* m_new2();
	void mark();
};
class c_Node13 : public Object{
	public:
	int m_key;
	c_Node13* m_right;
	c_Node13* m_left;
	c_KernPair* m_value;
	int m_color;
	c_Node13* m_parent;
	c_Node13();
	c_Node13* m_new(int,c_KernPair*,int,c_Node13*);
	c_Node13* m_new2();
	void mark();
};
class c_XMLError : public Object{
	public:
	bool m_error;
	String m_message;
	int m_line;
	int m_column;
	int m_offset;
	c_XMLError();
	c_XMLError* m_new();
	void p_Reset();
	void p_Set16(String,int,int,int);
	String p_ToString();
	void mark();
};
class c_XMLNode : public Object{
	public:
	String m_value;
	String m_name;
	bool m_valid;
	c_XMLDoc* m_doc;
	String m_path;
	c_List5* m_pathList;
	c_Node14* m_pathListNode;
	c_XMLNode* m_parent;
	int m_line;
	int m_column;
	int m_offset;
	c_StringMap6* m_attributes;
	c_XMLNode* m_lastChild;
	c_XMLNode* m_nextSibling;
	c_XMLNode* m_previousSibling;
	c_XMLNode* m_firstChild;
	c_List5* m_children;
	c_XMLNode();
	c_XMLNode* m_new(String,bool);
	c_XMLNode* m_new2();
	void p_SetAttribute3(String);
	void p_SetAttribute2(String,bool);
	void p_SetAttribute4(String,int);
	void p_SetAttribute5(String,Float);
	void p_SetAttribute(String,String);
	c_XMLNode* p_AddChild(String,String,String);
	c_List5* p_GetChildrenAtPath(String);
	c_XMLAttribute* p_GetXMLAttribute(String);
	c_List5* p_GetChildrenAtPath2(String,String);
	String p_GetAttribute(String);
	bool p_GetAttribute2(String,bool);
	int p_GetAttribute3(String,int);
	Float p_GetAttribute4(String,Float);
	String p_GetAttribute5(String,String);
	void mark();
};
class c_XMLDoc : public c_XMLNode{
	public:
	c_XMLNode* m_nullNode;
	String m_version;
	String m_encoding;
	c_StringMap5* m_paths;
	c_XMLDoc();
	c_XMLDoc* m_new(String,String,String);
	c_XMLDoc* m_new2();
	void mark();
};
class c_XMLStringBuffer : public Object{
	public:
	int m_chunk;
	int m_count;
	Array<int > m_data;
	int m_dirty;
	String m_cache;
	c_XMLStringBuffer();
	c_XMLStringBuffer* m_new(int);
	int p_Length();
	int p_Last2(int);
	void p_Add10(int);
	void p_Add11(String);
	void p_Add12(String,int,int);
	String p_value();
	void p_Clear();
	bool p_Trim();
	void mark();
};
class c_List5 : public Object{
	public:
	c_Node14* m__head;
	c_List5();
	c_List5* m_new();
	c_Node14* p_AddLast5(c_XMLNode*);
	c_List5* m_new2(Array<c_XMLNode* >);
	c_XMLNode* p_RemoveLast();
	bool p_Equals10(c_XMLNode*,c_XMLNode*);
	c_Node14* p_FindLast10(c_XMLNode*,c_Node14*);
	c_Node14* p_FindLast11(c_XMLNode*);
	void p_RemoveLast5(c_XMLNode*);
	bool p_IsEmpty();
	c_XMLNode* p_Last();
	c_Enumerator10* p_ObjectEnumerator();
	void mark();
};
class c_Node14 : public Object{
	public:
	c_Node14* m__succ;
	c_Node14* m__pred;
	c_XMLNode* m__data;
	c_Node14();
	c_Node14* m_new(c_Node14*,c_Node14*,c_XMLNode*);
	c_Node14* m_new2();
	int p_Remove2();
	void mark();
};
class c_HeadNode5 : public c_Node14{
	public:
	c_HeadNode5();
	c_HeadNode5* m_new();
	void mark();
};
bool bb_xml_XMLHasStringAtOffset(String,String,int);
class c_Map10 : public Object{
	public:
	c_Node15* m_root;
	c_Map10();
	c_Map10* m_new();
	virtual int p_Compare6(String,String)=0;
	int p_RotateLeft11(c_Node15*);
	int p_RotateRight10(c_Node15*);
	int p_InsertFixup10(c_Node15*);
	bool p_Set17(String,c_List5*);
	bool p_Insert14(String,c_List5*);
	c_Node15* p_FindNode3(String);
	c_List5* p_Get3(String);
	void mark();
};
class c_StringMap5 : public c_Map10{
	public:
	c_StringMap5();
	c_StringMap5* m_new();
	int p_Compare6(String,String);
	void mark();
};
class c_Node15 : public Object{
	public:
	String m_key;
	c_Node15* m_right;
	c_Node15* m_left;
	c_List5* m_value;
	int m_color;
	c_Node15* m_parent;
	c_Node15();
	c_Node15* m_new(String,c_List5*,int,c_Node15*);
	c_Node15* m_new2();
	void mark();
};
class c_XMLAttributeQuery : public Object{
	public:
	int m_count;
	Array<c_XMLAttributeQueryItem* > m_items;
	int m_chunk;
	c_XMLAttributeQuery();
	c_XMLAttributeQuery* m_new(String);
	c_XMLAttributeQuery* m_new2();
	int p_Length();
	bool p_Test(c_XMLNode*);
	void mark();
};
class c_XMLAttributeQueryItem : public Object{
	public:
	String m_id;
	String m_value;
	bool m_required;
	bool m_special;
	c_XMLAttributeQueryItem();
	c_XMLAttributeQueryItem* m_new(String,String,bool,bool);
	c_XMLAttributeQueryItem* m_new2();
	void mark();
};
class c_XMLAttribute : public Object{
	public:
	String m_id;
	String m_value;
	c_XMLAttribute();
	c_XMLAttribute* m_new(String,String);
	c_XMLAttribute* m_new2();
	void mark();
};
class c_Map11 : public Object{
	public:
	c_Node16* m_root;
	c_Map11();
	c_Map11* m_new();
	virtual int p_Compare6(String,String)=0;
	c_Node16* p_FindNode3(String);
	c_XMLAttribute* p_Get3(String);
	int p_RotateLeft12(c_Node16*);
	int p_RotateRight11(c_Node16*);
	int p_InsertFixup11(c_Node16*);
	bool p_Set18(String,c_XMLAttribute*);
	bool p_Insert15(String,c_XMLAttribute*);
	void mark();
};
class c_StringMap6 : public c_Map11{
	public:
	c_StringMap6();
	c_StringMap6* m_new();
	int p_Compare6(String,String);
	void mark();
};
class c_Node16 : public Object{
	public:
	String m_key;
	c_Node16* m_right;
	c_Node16* m_left;
	c_XMLAttribute* m_value;
	int m_color;
	c_Node16* m_parent;
	c_Node16();
	c_Node16* m_new(String,c_XMLAttribute*,int,c_Node16*);
	c_Node16* m_new2();
	void mark();
};
c_XMLDoc* bb_xml_ParseXML(String,c_XMLError*,int);
class c_Enumerator10 : public Object{
	public:
	c_List5* m__list;
	c_Node14* m__curr;
	c_Enumerator10();
	c_Enumerator10* m_new(c_List5*);
	c_Enumerator10* m_new2();
	bool p_HasNext();
	c_XMLNode* p_NextObject();
	void mark();
};
int bb_app_Millisecs();
extern int bb_fps_startTime;
extern int bb_fps_fpsCount;
extern int bb_fps_currentRate;
void bb_fps_UpdateFps();
void bb_functions_ResetMatrix();
int bb_fps_GetFps();
int bb_graphics_DrawImageRect(c_Image*,Float,Float,int,int,int,int,int);
int bb_graphics_DrawImageRect2(c_Image*,Float,Float,int,int,int,int,Float,Float,Float,int);
int bb_input_KeyHit(int);
class c_VAction : public Object{
	public:
	Float m_duration;
	bool m_active;
	c_VActionEventHandler* m_listener;
	c_Node17* m_link;
	Float m_time;
	c_VAction();
	c_VAction* m_new();
	void p_Start();
	void p_AddToList(c_List6*);
	void p_SetListener(c_VActionEventHandler*);
	virtual void p_Update4(Float);
	void p_IncrementTime(Float);
	void p_Stop();
	void mark();
};
class c_VVec2Action : public c_VAction{
	public:
	c_Vec2* m_pointer;
	c_Vec2* m_moveBy;
	int m_easingType;
	c_Vec2* m_startPosition;
	c_Vec2* m_lastPosition;
	c_VVec2Action();
	c_VVec2Action* m_new(c_Vec2*,Float,Float,Float,int,bool);
	c_VVec2Action* m_new2();
	void p_Update4(Float);
	void mark();
};
class c_List6 : public Object{
	public:
	c_Node17* m__head;
	c_List6();
	c_List6* m_new();
	c_Node17* p_AddLast6(c_VAction*);
	c_List6* m_new2(Array<c_VAction* >);
	c_Enumerator11* p_ObjectEnumerator();
	void mark();
};
class c_Node17 : public Object{
	public:
	c_Node17* m__succ;
	c_Node17* m__pred;
	c_VAction* m__data;
	c_Node17();
	c_Node17* m_new(c_Node17*,c_Node17*,c_VAction*);
	c_Node17* m_new2();
	int p_Remove2();
	void mark();
};
class c_HeadNode6 : public c_Node17{
	public:
	c_HeadNode6();
	c_HeadNode6* m_new();
	void mark();
};
class c_Enumerator11 : public Object{
	public:
	c_List6* m__list;
	c_Node17* m__curr;
	c_Enumerator11();
	c_Enumerator11* m_new(c_List6*);
	c_Enumerator11* m_new2();
	bool p_HasNext();
	c_VAction* p_NextObject();
	void mark();
};
int bb_graphics_Cls(Float,Float,Float);
void bb_functions_ClearScreenWithColor(c_Color*);
class c_Enumerator12 : public Object{
	public:
	c_List4* m__list;
	c_Node10* m__curr;
	c_Enumerator12();
	c_Enumerator12* m_new(c_List4*);
	c_Enumerator12* m_new2();
	bool p_HasNext();
	c_VShape* p_NextObject();
	void mark();
};
int bb_graphics_GetBlend();
void bb_functions_ResetBlend();
extern bool bb_ease_initialized;
class c_Tweener : public virtual gc_interface{
	public:
	virtual Float p_Do(Float,Float,Float,Float)=0;
};
class c_LinearTween : public Object,public virtual c_Tweener{
	public:
	c_LinearTween();
	c_LinearTween* m_new();
	Float p_Do(Float,Float,Float,Float);
	void mark();
};
extern Array<c_Tweener* > bb_ease_TweenFunc;
class c_EaseInQuad : public Object,public virtual c_Tweener{
	public:
	c_EaseInQuad();
	c_EaseInQuad* m_new();
	Float p_Do(Float,Float,Float,Float);
	void mark();
};
class c_EaseOutQuad : public Object,public virtual c_Tweener{
	public:
	c_EaseOutQuad();
	c_EaseOutQuad* m_new();
	Float p_Do(Float,Float,Float,Float);
	void mark();
};
class c_EaseInOutQuad : public Object,public virtual c_Tweener{
	public:
	c_EaseInOutQuad();
	c_EaseInOutQuad* m_new();
	Float p_Do(Float,Float,Float,Float);
	void mark();
};
class c_EaseInCubic : public Object,public virtual c_Tweener{
	public:
	c_EaseInCubic();
	c_EaseInCubic* m_new();
	Float p_Do(Float,Float,Float,Float);
	void mark();
};
class c_EaseOutCubic : public Object,public virtual c_Tweener{
	public:
	c_EaseOutCubic();
	c_EaseOutCubic* m_new();
	Float p_Do(Float,Float,Float,Float);
	void mark();
};
class c_EaseInOutCubic : public Object,public virtual c_Tweener{
	public:
	c_EaseInOutCubic();
	c_EaseInOutCubic* m_new();
	Float p_Do(Float,Float,Float,Float);
	void mark();
};
class c_EaseInQuart : public Object,public virtual c_Tweener{
	public:
	c_EaseInQuart();
	c_EaseInQuart* m_new();
	Float p_Do(Float,Float,Float,Float);
	void mark();
};
class c_EaseOutQuart : public Object,public virtual c_Tweener{
	public:
	c_EaseOutQuart();
	c_EaseOutQuart* m_new();
	Float p_Do(Float,Float,Float,Float);
	void mark();
};
class c_EaseInOutQuart : public Object,public virtual c_Tweener{
	public:
	c_EaseInOutQuart();
	c_EaseInOutQuart* m_new();
	Float p_Do(Float,Float,Float,Float);
	void mark();
};
class c_EaseInQuint : public Object,public virtual c_Tweener{
	public:
	c_EaseInQuint();
	c_EaseInQuint* m_new();
	Float p_Do(Float,Float,Float,Float);
	void mark();
};
class c_EaseOutQuint : public Object,public virtual c_Tweener{
	public:
	c_EaseOutQuint();
	c_EaseOutQuint* m_new();
	Float p_Do(Float,Float,Float,Float);
	void mark();
};
class c_EaseInOutQuint : public Object,public virtual c_Tweener{
	public:
	c_EaseInOutQuint();
	c_EaseInOutQuint* m_new();
	Float p_Do(Float,Float,Float,Float);
	void mark();
};
class c_EaseInSine : public Object,public virtual c_Tweener{
	public:
	c_EaseInSine();
	c_EaseInSine* m_new();
	Float p_Do(Float,Float,Float,Float);
	void mark();
};
class c_EaseOutSine : public Object,public virtual c_Tweener{
	public:
	c_EaseOutSine();
	c_EaseOutSine* m_new();
	Float p_Do(Float,Float,Float,Float);
	void mark();
};
class c_EaseInOutSine : public Object,public virtual c_Tweener{
	public:
	c_EaseInOutSine();
	c_EaseInOutSine* m_new();
	Float p_Do(Float,Float,Float,Float);
	void mark();
};
class c_EaseInExpo : public Object,public virtual c_Tweener{
	public:
	c_EaseInExpo();
	c_EaseInExpo* m_new();
	Float p_Do(Float,Float,Float,Float);
	void mark();
};
class c_EaseOutExpo : public Object,public virtual c_Tweener{
	public:
	c_EaseOutExpo();
	c_EaseOutExpo* m_new();
	Float p_Do(Float,Float,Float,Float);
	void mark();
};
class c_EaseInOutExpo : public Object,public virtual c_Tweener{
	public:
	c_EaseInOutExpo();
	c_EaseInOutExpo* m_new();
	Float p_Do(Float,Float,Float,Float);
	void mark();
};
class c_EaseInCirc : public Object,public virtual c_Tweener{
	public:
	c_EaseInCirc();
	c_EaseInCirc* m_new();
	Float p_Do(Float,Float,Float,Float);
	void mark();
};
class c_EaseOutCirc : public Object,public virtual c_Tweener{
	public:
	c_EaseOutCirc();
	c_EaseOutCirc* m_new();
	Float p_Do(Float,Float,Float,Float);
	void mark();
};
class c_EaseInOutCirc : public Object,public virtual c_Tweener{
	public:
	c_EaseInOutCirc();
	c_EaseInOutCirc* m_new();
	Float p_Do(Float,Float,Float,Float);
	void mark();
};
class c_EaseInBack : public Object,public virtual c_Tweener{
	public:
	c_EaseInBack();
	c_EaseInBack* m_new();
	Float p_Do(Float,Float,Float,Float);
	void mark();
};
class c_EaseOutBack : public Object,public virtual c_Tweener{
	public:
	c_EaseOutBack();
	c_EaseOutBack* m_new();
	Float p_Do(Float,Float,Float,Float);
	void mark();
};
class c_EaseInOutBack : public Object,public virtual c_Tweener{
	public:
	c_EaseInOutBack();
	c_EaseInOutBack* m_new();
	Float p_Do(Float,Float,Float,Float);
	void mark();
};
class c_EaseInBounce : public Object,public virtual c_Tweener{
	public:
	c_EaseInBounce();
	c_EaseInBounce* m_new();
	Float p_Do(Float,Float,Float,Float);
	void mark();
};
class c_EaseOutBounce : public Object,public virtual c_Tweener{
	public:
	c_EaseOutBounce();
	c_EaseOutBounce* m_new();
	Float p_Do(Float,Float,Float,Float);
	void mark();
};
class c_EaseInOutBounce : public Object,public virtual c_Tweener{
	public:
	c_EaseInOutBounce();
	c_EaseInOutBounce* m_new();
	Float p_Do(Float,Float,Float,Float);
	void mark();
};
class c_EaseInElastic : public Object,public virtual c_Tweener{
	public:
	c_EaseInElastic();
	c_EaseInElastic* m_new();
	Float p_Do(Float,Float,Float,Float);
	void mark();
};
class c_EaseOutElastic : public Object,public virtual c_Tweener{
	public:
	c_EaseOutElastic();
	c_EaseOutElastic* m_new();
	Float p_Do(Float,Float,Float,Float);
	void mark();
};
class c_EaseInOutElastic : public Object,public virtual c_Tweener{
	public:
	c_EaseInOutElastic();
	c_EaseInOutElastic* m_new();
	Float p_Do(Float,Float,Float,Float);
	void mark();
};
void bb_ease_InitTweenSystem();
Float bb_ease_Tweening(int,Float,Float,Float,Float);
extern Float bb_ease_Bounce;
extern Float bb_ease_Power;
extern Float bb_ease_Amplitude;
c_BoolObject::c_BoolObject(){
	m_value=false;
}
c_BoolObject* c_BoolObject::m_new(bool t_value){
	this->m_value=t_value;
	return this;
}
bool c_BoolObject::p_ToBool(){
	return m_value;
}
bool c_BoolObject::p_Equals(c_BoolObject* t_box){
	return m_value==t_box->m_value;
}
c_BoolObject* c_BoolObject::m_new2(){
	return this;
}
void c_BoolObject::mark(){
	Object::mark();
}
c_IntObject::c_IntObject(){
	m_value=0;
}
c_IntObject* c_IntObject::m_new(int t_value){
	this->m_value=t_value;
	return this;
}
c_IntObject* c_IntObject::m_new2(Float t_value){
	this->m_value=int(t_value);
	return this;
}
int c_IntObject::p_ToInt(){
	return m_value;
}
Float c_IntObject::p_ToFloat(){
	return Float(m_value);
}
String c_IntObject::p_ToString(){
	return String(m_value);
}
bool c_IntObject::p_Equals2(c_IntObject* t_box){
	return m_value==t_box->m_value;
}
int c_IntObject::p_Compare(c_IntObject* t_box){
	return m_value-t_box->m_value;
}
c_IntObject* c_IntObject::m_new3(){
	return this;
}
void c_IntObject::mark(){
	Object::mark();
}
c_FloatObject::c_FloatObject(){
	m_value=FLOAT(.0);
}
c_FloatObject* c_FloatObject::m_new(int t_value){
	this->m_value=Float(t_value);
	return this;
}
c_FloatObject* c_FloatObject::m_new2(Float t_value){
	this->m_value=t_value;
	return this;
}
int c_FloatObject::p_ToInt(){
	return int(m_value);
}
Float c_FloatObject::p_ToFloat(){
	return m_value;
}
String c_FloatObject::p_ToString(){
	return String(m_value);
}
bool c_FloatObject::p_Equals3(c_FloatObject* t_box){
	return m_value==t_box->m_value;
}
int c_FloatObject::p_Compare2(c_FloatObject* t_box){
	if(m_value<t_box->m_value){
		return -1;
	}
	return ((m_value>t_box->m_value)?1:0);
}
c_FloatObject* c_FloatObject::m_new3(){
	return this;
}
void c_FloatObject::mark(){
	Object::mark();
}
c_StringObject::c_StringObject(){
	m_value=String();
}
c_StringObject* c_StringObject::m_new(int t_value){
	this->m_value=String(t_value);
	return this;
}
c_StringObject* c_StringObject::m_new2(Float t_value){
	this->m_value=String(t_value);
	return this;
}
c_StringObject* c_StringObject::m_new3(String t_value){
	this->m_value=t_value;
	return this;
}
String c_StringObject::p_ToString(){
	return m_value;
}
bool c_StringObject::p_Equals4(c_StringObject* t_box){
	return m_value==t_box->m_value;
}
int c_StringObject::p_Compare3(c_StringObject* t_box){
	return m_value.Compare(t_box->m_value);
}
c_StringObject* c_StringObject::m_new4(){
	return this;
}
void c_StringObject::mark(){
	Object::mark();
}
Object* bb_boxes_BoxBool(bool t_value){
	return ((new c_BoolObject)->m_new(t_value));
}
Object* bb_boxes_BoxInt(int t_value){
	return ((new c_IntObject)->m_new(t_value));
}
Object* bb_boxes_BoxFloat(Float t_value){
	return ((new c_FloatObject)->m_new2(t_value));
}
Object* bb_boxes_BoxString(String t_value){
	return ((new c_StringObject)->m_new3(t_value));
}
bool bb_boxes_UnboxBool(Object* t_box){
	return dynamic_cast<c_BoolObject*>(t_box)->m_value;
}
int bb_boxes_UnboxInt(Object* t_box){
	return dynamic_cast<c_IntObject*>(t_box)->m_value;
}
Float bb_boxes_UnboxFloat(Object* t_box){
	return dynamic_cast<c_FloatObject*>(t_box)->m_value;
}
String bb_boxes_UnboxString(Object* t_box){
	return dynamic_cast<c_StringObject*>(t_box)->m_value;
}
c_Deque::c_Deque(){
	m__data=Array<int >(4);
	m__capacity=0;
	m__last=0;
	m__first=0;
}
c_Deque* c_Deque::m_new(){
	return this;
}
c_Deque* c_Deque::m_new2(Array<int > t_arr){
	gc_assign(m__data,t_arr.Slice(0));
	m__capacity=m__data.Length();
	m__last=m__capacity;
	return this;
}
int c_Deque::m_NIL;
void c_Deque::p_Clear(){
	if(m__first<=m__last){
		for(int t_i=m__first;t_i<m__last;t_i=t_i+1){
			m__data[t_i]=m_NIL;
		}
	}else{
		for(int t_i2=0;t_i2<m__last;t_i2=t_i2+1){
			m__data[t_i2]=m_NIL;
		}
		for(int t_i3=m__first;t_i3<m__capacity;t_i3=t_i3+1){
			m__data[t_i3]=m_NIL;
		}
	}
	m__first=0;
	m__last=0;
}
int c_Deque::p_Length(){
	if(m__last>=m__first){
		return m__last-m__first;
	}
	return m__capacity-m__first+m__last;
}
bool c_Deque::p_IsEmpty(){
	return m__first==m__last;
}
Array<int > c_Deque::p_ToArray(){
	Array<int > t_data=Array<int >(p_Length());
	if(m__first<=m__last){
		for(int t_i=m__first;t_i<m__last;t_i=t_i+1){
			t_data[t_i-m__first]=m__data[t_i];
		}
	}else{
		int t_n=m__capacity-m__first;
		for(int t_i2=0;t_i2<t_n;t_i2=t_i2+1){
			t_data[t_i2]=m__data[m__first+t_i2];
		}
		for(int t_i3=0;t_i3<m__last;t_i3=t_i3+1){
			t_data[t_n+t_i3]=m__data[t_i3];
		}
	}
	return t_data;
}
c_Enumerator2* c_Deque::p_ObjectEnumerator(){
	return (new c_Enumerator2)->m_new(this);
}
int c_Deque::p_Get(int t_index){
	return m__data[(t_index+m__first) % m__capacity];
}
void c_Deque::p_Set(int t_index,int t_value){
	m__data[(t_index+m__first) % m__capacity]=t_value;
}
void c_Deque::p_Grow(){
	Array<int > t_data=Array<int >(m__capacity*2+10);
	if(m__first<=m__last){
		for(int t_i=m__first;t_i<m__last;t_i=t_i+1){
			t_data[t_i-m__first]=m__data[t_i];
		}
		m__last-=m__first;
		m__first=0;
	}else{
		int t_n=m__capacity-m__first;
		for(int t_i2=0;t_i2<t_n;t_i2=t_i2+1){
			t_data[t_i2]=m__data[m__first+t_i2];
		}
		for(int t_i3=0;t_i3<m__last;t_i3=t_i3+1){
			t_data[t_n+t_i3]=m__data[t_i3];
		}
		m__last+=t_n;
		m__first=0;
	}
	m__capacity=t_data.Length();
	gc_assign(m__data,t_data);
}
void c_Deque::p_PushFirst(int t_value){
	if(p_Length()+1>=m__capacity){
		p_Grow();
	}
	m__first-=1;
	if(m__first<0){
		m__first=m__capacity-1;
	}
	m__data[m__first]=t_value;
}
void c_Deque::p_PushLast(int t_value){
	if(p_Length()+1>=m__capacity){
		p_Grow();
	}
	m__data[m__last]=t_value;
	m__last+=1;
	if(m__last==m__capacity){
		m__last=0;
	}
}
int c_Deque::p_PopFirst(){
	int t_v=m__data[m__first];
	m__data[m__first]=m_NIL;
	m__first+=1;
	if(m__first==m__capacity){
		m__first=0;
	}
	return t_v;
}
int c_Deque::p_PopLast(){
	if(m__last==0){
		m__last=m__capacity;
	}
	m__last-=1;
	int t_v=m__data[m__last];
	m__data[m__last]=m_NIL;
	return t_v;
}
int c_Deque::p_First(){
	return m__data[m__first];
}
int c_Deque::p_Last(){
	return m__data[(m__last-1) % m__capacity];
}
void c_Deque::mark(){
	Object::mark();
	gc_mark_q(m__data);
}
c_IntDeque::c_IntDeque(){
}
c_IntDeque* c_IntDeque::m_new(){
	c_Deque::m_new();
	return this;
}
c_IntDeque* c_IntDeque::m_new2(Array<int > t_data){
	c_Deque::m_new2(t_data);
	return this;
}
void c_IntDeque::mark(){
	c_Deque::mark();
}
c_Deque2::c_Deque2(){
	m__data=Array<Float >(4);
	m__capacity=0;
	m__last=0;
	m__first=0;
}
c_Deque2* c_Deque2::m_new(){
	return this;
}
c_Deque2* c_Deque2::m_new2(Array<Float > t_arr){
	gc_assign(m__data,t_arr.Slice(0));
	m__capacity=m__data.Length();
	m__last=m__capacity;
	return this;
}
Float c_Deque2::m_NIL;
void c_Deque2::p_Clear(){
	if(m__first<=m__last){
		for(int t_i=m__first;t_i<m__last;t_i=t_i+1){
			m__data[t_i]=m_NIL;
		}
	}else{
		for(int t_i2=0;t_i2<m__last;t_i2=t_i2+1){
			m__data[t_i2]=m_NIL;
		}
		for(int t_i3=m__first;t_i3<m__capacity;t_i3=t_i3+1){
			m__data[t_i3]=m_NIL;
		}
	}
	m__first=0;
	m__last=0;
}
int c_Deque2::p_Length(){
	if(m__last>=m__first){
		return m__last-m__first;
	}
	return m__capacity-m__first+m__last;
}
bool c_Deque2::p_IsEmpty(){
	return m__first==m__last;
}
Array<Float > c_Deque2::p_ToArray(){
	Array<Float > t_data=Array<Float >(p_Length());
	if(m__first<=m__last){
		for(int t_i=m__first;t_i<m__last;t_i=t_i+1){
			t_data[t_i-m__first]=m__data[t_i];
		}
	}else{
		int t_n=m__capacity-m__first;
		for(int t_i2=0;t_i2<t_n;t_i2=t_i2+1){
			t_data[t_i2]=m__data[m__first+t_i2];
		}
		for(int t_i3=0;t_i3<m__last;t_i3=t_i3+1){
			t_data[t_n+t_i3]=m__data[t_i3];
		}
	}
	return t_data;
}
c_Enumerator3* c_Deque2::p_ObjectEnumerator(){
	return (new c_Enumerator3)->m_new(this);
}
Float c_Deque2::p_Get(int t_index){
	return m__data[(t_index+m__first) % m__capacity];
}
void c_Deque2::p_Set2(int t_index,Float t_value){
	m__data[(t_index+m__first) % m__capacity]=t_value;
}
void c_Deque2::p_Grow(){
	Array<Float > t_data=Array<Float >(m__capacity*2+10);
	if(m__first<=m__last){
		for(int t_i=m__first;t_i<m__last;t_i=t_i+1){
			t_data[t_i-m__first]=m__data[t_i];
		}
		m__last-=m__first;
		m__first=0;
	}else{
		int t_n=m__capacity-m__first;
		for(int t_i2=0;t_i2<t_n;t_i2=t_i2+1){
			t_data[t_i2]=m__data[m__first+t_i2];
		}
		for(int t_i3=0;t_i3<m__last;t_i3=t_i3+1){
			t_data[t_n+t_i3]=m__data[t_i3];
		}
		m__last+=t_n;
		m__first=0;
	}
	m__capacity=t_data.Length();
	gc_assign(m__data,t_data);
}
void c_Deque2::p_PushFirst2(Float t_value){
	if(p_Length()+1>=m__capacity){
		p_Grow();
	}
	m__first-=1;
	if(m__first<0){
		m__first=m__capacity-1;
	}
	m__data[m__first]=t_value;
}
void c_Deque2::p_PushLast2(Float t_value){
	if(p_Length()+1>=m__capacity){
		p_Grow();
	}
	m__data[m__last]=t_value;
	m__last+=1;
	if(m__last==m__capacity){
		m__last=0;
	}
}
Float c_Deque2::p_PopFirst(){
	Float t_v=m__data[m__first];
	m__data[m__first]=m_NIL;
	m__first+=1;
	if(m__first==m__capacity){
		m__first=0;
	}
	return t_v;
}
Float c_Deque2::p_PopLast(){
	if(m__last==0){
		m__last=m__capacity;
	}
	m__last-=1;
	Float t_v=m__data[m__last];
	m__data[m__last]=m_NIL;
	return t_v;
}
Float c_Deque2::p_First(){
	return m__data[m__first];
}
Float c_Deque2::p_Last(){
	return m__data[(m__last-1) % m__capacity];
}
void c_Deque2::mark(){
	Object::mark();
	gc_mark_q(m__data);
}
c_FloatDeque::c_FloatDeque(){
}
c_FloatDeque* c_FloatDeque::m_new(){
	c_Deque2::m_new();
	return this;
}
c_FloatDeque* c_FloatDeque::m_new2(Array<Float > t_data){
	c_Deque2::m_new2(t_data);
	return this;
}
void c_FloatDeque::mark(){
	c_Deque2::mark();
}
c_Deque3::c_Deque3(){
	m__data=Array<String >(4);
	m__capacity=0;
	m__last=0;
	m__first=0;
}
c_Deque3* c_Deque3::m_new(){
	return this;
}
c_Deque3* c_Deque3::m_new2(Array<String > t_arr){
	gc_assign(m__data,t_arr.Slice(0));
	m__capacity=m__data.Length();
	m__last=m__capacity;
	return this;
}
String c_Deque3::m_NIL;
void c_Deque3::p_Clear(){
	if(m__first<=m__last){
		for(int t_i=m__first;t_i<m__last;t_i=t_i+1){
			m__data[t_i]=m_NIL;
		}
	}else{
		for(int t_i2=0;t_i2<m__last;t_i2=t_i2+1){
			m__data[t_i2]=m_NIL;
		}
		for(int t_i3=m__first;t_i3<m__capacity;t_i3=t_i3+1){
			m__data[t_i3]=m_NIL;
		}
	}
	m__first=0;
	m__last=0;
}
int c_Deque3::p_Length(){
	if(m__last>=m__first){
		return m__last-m__first;
	}
	return m__capacity-m__first+m__last;
}
bool c_Deque3::p_IsEmpty(){
	return m__first==m__last;
}
Array<String > c_Deque3::p_ToArray(){
	Array<String > t_data=Array<String >(p_Length());
	if(m__first<=m__last){
		for(int t_i=m__first;t_i<m__last;t_i=t_i+1){
			t_data[t_i-m__first]=m__data[t_i];
		}
	}else{
		int t_n=m__capacity-m__first;
		for(int t_i2=0;t_i2<t_n;t_i2=t_i2+1){
			t_data[t_i2]=m__data[m__first+t_i2];
		}
		for(int t_i3=0;t_i3<m__last;t_i3=t_i3+1){
			t_data[t_n+t_i3]=m__data[t_i3];
		}
	}
	return t_data;
}
c_Enumerator4* c_Deque3::p_ObjectEnumerator(){
	return (new c_Enumerator4)->m_new(this);
}
String c_Deque3::p_Get(int t_index){
	return m__data[(t_index+m__first) % m__capacity];
}
void c_Deque3::p_Set3(int t_index,String t_value){
	m__data[(t_index+m__first) % m__capacity]=t_value;
}
void c_Deque3::p_Grow(){
	Array<String > t_data=Array<String >(m__capacity*2+10);
	if(m__first<=m__last){
		for(int t_i=m__first;t_i<m__last;t_i=t_i+1){
			t_data[t_i-m__first]=m__data[t_i];
		}
		m__last-=m__first;
		m__first=0;
	}else{
		int t_n=m__capacity-m__first;
		for(int t_i2=0;t_i2<t_n;t_i2=t_i2+1){
			t_data[t_i2]=m__data[m__first+t_i2];
		}
		for(int t_i3=0;t_i3<m__last;t_i3=t_i3+1){
			t_data[t_n+t_i3]=m__data[t_i3];
		}
		m__last+=t_n;
		m__first=0;
	}
	m__capacity=t_data.Length();
	gc_assign(m__data,t_data);
}
void c_Deque3::p_PushFirst3(String t_value){
	if(p_Length()+1>=m__capacity){
		p_Grow();
	}
	m__first-=1;
	if(m__first<0){
		m__first=m__capacity-1;
	}
	m__data[m__first]=t_value;
}
void c_Deque3::p_PushLast3(String t_value){
	if(p_Length()+1>=m__capacity){
		p_Grow();
	}
	m__data[m__last]=t_value;
	m__last+=1;
	if(m__last==m__capacity){
		m__last=0;
	}
}
String c_Deque3::p_PopFirst(){
	String t_v=m__data[m__first];
	m__data[m__first]=m_NIL;
	m__first+=1;
	if(m__first==m__capacity){
		m__first=0;
	}
	return t_v;
}
String c_Deque3::p_PopLast(){
	if(m__last==0){
		m__last=m__capacity;
	}
	m__last-=1;
	String t_v=m__data[m__last];
	m__data[m__last]=m_NIL;
	return t_v;
}
String c_Deque3::p_First(){
	return m__data[m__first];
}
String c_Deque3::p_Last(){
	return m__data[(m__last-1) % m__capacity];
}
void c_Deque3::mark(){
	Object::mark();
	gc_mark_q(m__data);
}
c_StringDeque::c_StringDeque(){
}
c_StringDeque* c_StringDeque::m_new(){
	c_Deque3::m_new();
	return this;
}
c_StringDeque* c_StringDeque::m_new2(Array<String > t_data){
	c_Deque3::m_new2(t_data);
	return this;
}
void c_StringDeque::mark(){
	c_Deque3::mark();
}
c_List::c_List(){
	m__head=((new c_HeadNode)->m_new());
}
c_List* c_List::m_new(){
	return this;
}
c_Node* c_List::p_AddLast(int t_data){
	return (new c_Node)->m_new(m__head,m__head->m__pred,t_data);
}
c_List* c_List::m_new2(Array<int > t_data){
	Array<int > t_=t_data;
	int t_2=0;
	while(t_2<t_.Length()){
		int t_t=t_[t_2];
		t_2=t_2+1;
		p_AddLast(t_t);
	}
	return this;
}
bool c_List::p_Equals5(int t_lhs,int t_rhs){
	return t_lhs==t_rhs;
}
int c_List::p_Compare4(int t_lhs,int t_rhs){
	bbError(String(L"Unable to compare items",23));
	return 0;
}
int c_List::p_Count(){
	int t_n=0;
	c_Node* t_node=m__head->m__succ;
	while(t_node!=m__head){
		t_node=t_node->m__succ;
		t_n+=1;
	}
	return t_n;
}
c_Enumerator5* c_List::p_ObjectEnumerator(){
	return (new c_Enumerator5)->m_new(this);
}
Array<int > c_List::p_ToArray(){
	Array<int > t_arr=Array<int >(p_Count());
	int t_i=0;
	c_Enumerator5* t_=this->p_ObjectEnumerator();
	while(t_->p_HasNext()){
		int t_t=t_->p_NextObject();
		t_arr[t_i]=t_t;
		t_i+=1;
	}
	return t_arr;
}
int c_List::p_Clear(){
	gc_assign(m__head->m__succ,m__head);
	gc_assign(m__head->m__pred,m__head);
	return 0;
}
bool c_List::p_IsEmpty(){
	return m__head->m__succ==m__head;
}
bool c_List::p_Contains(int t_value){
	c_Node* t_node=m__head->m__succ;
	while(t_node!=m__head){
		if(p_Equals5(t_node->m__data,t_value)){
			return true;
		}
		t_node=t_node->m__succ;
	}
	return false;
}
c_Node* c_List::p_FirstNode(){
	if(m__head->m__succ!=m__head){
		return m__head->m__succ;
	}
	return 0;
}
c_Node* c_List::p_LastNode(){
	if(m__head->m__pred!=m__head){
		return m__head->m__pred;
	}
	return 0;
}
int c_List::p_First(){
	return m__head->m__succ->m__data;
}
int c_List::p_Last(){
	return m__head->m__pred->m__data;
}
int c_List::p_RemoveFirst(){
	int t_data=m__head->m__succ->m__data;
	m__head->m__succ->p_Remove2();
	return t_data;
}
int c_List::p_RemoveLast(){
	int t_data=m__head->m__pred->m__data;
	m__head->m__pred->p_Remove2();
	return t_data;
}
c_Node* c_List::p_AddFirst(int t_data){
	return (new c_Node)->m_new(m__head->m__succ,m__head,t_data);
}
c_Node* c_List::p_Find(int t_value,c_Node* t_start){
	while(t_start!=m__head){
		if(p_Equals5(t_value,t_start->m__data)){
			return t_start;
		}
		t_start=t_start->m__succ;
	}
	return 0;
}
c_Node* c_List::p_Find2(int t_value){
	return p_Find(t_value,m__head->m__succ);
}
c_Node* c_List::p_FindLast(int t_value,c_Node* t_start){
	while(t_start!=m__head){
		if(p_Equals5(t_value,t_start->m__data)){
			return t_start;
		}
		t_start=t_start->m__pred;
	}
	return 0;
}
c_Node* c_List::p_FindLast2(int t_value){
	return p_FindLast(t_value,m__head->m__pred);
}
int c_List::p_RemoveEach(int t_value){
	c_Node* t_node=m__head->m__succ;
	while(t_node!=m__head){
		c_Node* t_succ=t_node->m__succ;
		if(p_Equals5(t_node->m__data,t_value)){
			t_node->p_Remove2();
		}
		t_node=t_succ;
	}
	return 0;
}
void c_List::p_Remove(int t_value){
	p_RemoveEach(t_value);
}
void c_List::p_RemoveFirst2(int t_value){
	c_Node* t_node=p_Find2(t_value);
	if((t_node)!=0){
		t_node->p_Remove2();
	}
}
void c_List::p_RemoveLast2(int t_value){
	c_Node* t_node=p_FindLast2(t_value);
	if((t_node)!=0){
		t_node->p_Remove2();
	}
}
c_Node* c_List::p_InsertBefore(int t_where,int t_data){
	c_Node* t_node=p_Find2(t_where);
	if((t_node)!=0){
		return (new c_Node)->m_new(t_node,t_node->m__pred,t_data);
	}
	return 0;
}
c_Node* c_List::p_InsertAfter(int t_where,int t_data){
	c_Node* t_node=p_Find2(t_where);
	if((t_node)!=0){
		return (new c_Node)->m_new(t_node->m__succ,t_node,t_data);
	}
	return 0;
}
void c_List::p_InsertBeforeEach(int t_where,int t_data){
	c_Node* t_node=p_Find2(t_where);
	while((t_node)!=0){
		(new c_Node)->m_new(t_node,t_node->m__pred,t_data);
		t_node=p_Find(t_where,t_node->m__succ);
	}
}
void c_List::p_InsertAfterEach(int t_where,int t_data){
	c_Node* t_node=p_Find2(t_where);
	while((t_node)!=0){
		t_node=(new c_Node)->m_new(t_node->m__succ,t_node,t_data);
		t_node=p_Find(t_where,t_node->m__succ);
	}
}
c_BackwardsList* c_List::p_Backwards(){
	return (new c_BackwardsList)->m_new(this);
}
int c_List::p_Sort(int t_ascending){
	int t_ccsgn=-1;
	if((t_ascending)!=0){
		t_ccsgn=1;
	}
	int t_insize=1;
	do{
		int t_merges=0;
		c_Node* t_tail=m__head;
		c_Node* t_p=m__head->m__succ;
		while(t_p!=m__head){
			t_merges+=1;
			c_Node* t_q=t_p->m__succ;
			int t_qsize=t_insize;
			int t_psize=1;
			while(t_psize<t_insize && t_q!=m__head){
				t_psize+=1;
				t_q=t_q->m__succ;
			}
			do{
				c_Node* t_t=0;
				if(((t_psize)!=0) && ((t_qsize)!=0) && t_q!=m__head){
					int t_cc=p_Compare4(t_p->m__data,t_q->m__data)*t_ccsgn;
					if(t_cc<=0){
						t_t=t_p;
						t_p=t_p->m__succ;
						t_psize-=1;
					}else{
						t_t=t_q;
						t_q=t_q->m__succ;
						t_qsize-=1;
					}
				}else{
					if((t_psize)!=0){
						t_t=t_p;
						t_p=t_p->m__succ;
						t_psize-=1;
					}else{
						if(((t_qsize)!=0) && t_q!=m__head){
							t_t=t_q;
							t_q=t_q->m__succ;
							t_qsize-=1;
						}else{
							break;
						}
					}
				}
				gc_assign(t_t->m__pred,t_tail);
				gc_assign(t_tail->m__succ,t_t);
				t_tail=t_t;
			}while(!(false));
			t_p=t_q;
		}
		gc_assign(t_tail->m__succ,m__head);
		gc_assign(m__head->m__pred,t_tail);
		if(t_merges<=1){
			return 0;
		}
		t_insize*=2;
	}while(!(false));
}
void c_List::mark(){
	Object::mark();
	gc_mark_q(m__head);
}
c_IntList::c_IntList(){
}
c_IntList* c_IntList::m_new(Array<int > t_data){
	c_List::m_new2(t_data);
	return this;
}
bool c_IntList::p_Equals5(int t_lhs,int t_rhs){
	return t_lhs==t_rhs;
}
int c_IntList::p_Compare4(int t_lhs,int t_rhs){
	return t_lhs-t_rhs;
}
c_IntList* c_IntList::m_new2(){
	c_List::m_new();
	return this;
}
void c_IntList::mark(){
	c_List::mark();
}
c_Node::c_Node(){
	m__succ=0;
	m__pred=0;
	m__data=0;
}
c_Node* c_Node::m_new(c_Node* t_succ,c_Node* t_pred,int t_data){
	gc_assign(m__succ,t_succ);
	gc_assign(m__pred,t_pred);
	gc_assign(m__succ->m__pred,this);
	gc_assign(m__pred->m__succ,this);
	m__data=t_data;
	return this;
}
c_Node* c_Node::m_new2(){
	return this;
}
int c_Node::p_Remove2(){
	gc_assign(m__succ->m__pred,m__pred);
	gc_assign(m__pred->m__succ,m__succ);
	return 0;
}
int c_Node::p_Value(){
	return m__data;
}
c_Node* c_Node::p_GetNode(){
	return this;
}
c_Node* c_Node::p_NextNode(){
	return m__succ->p_GetNode();
}
c_Node* c_Node::p_PrevNode(){
	return m__pred->p_GetNode();
}
void c_Node::mark(){
	Object::mark();
	gc_mark_q(m__succ);
	gc_mark_q(m__pred);
}
c_HeadNode::c_HeadNode(){
}
c_HeadNode* c_HeadNode::m_new(){
	c_Node::m_new2();
	gc_assign(m__succ,(this));
	gc_assign(m__pred,(this));
	return this;
}
c_Node* c_HeadNode::p_GetNode(){
	return 0;
}
void c_HeadNode::mark(){
	c_Node::mark();
}
c_List2::c_List2(){
	m__head=((new c_HeadNode2)->m_new());
}
c_List2* c_List2::m_new(){
	return this;
}
c_Node2* c_List2::p_AddLast2(Float t_data){
	return (new c_Node2)->m_new(m__head,m__head->m__pred,t_data);
}
c_List2* c_List2::m_new2(Array<Float > t_data){
	Array<Float > t_=t_data;
	int t_2=0;
	while(t_2<t_.Length()){
		Float t_t=t_[t_2];
		t_2=t_2+1;
		p_AddLast2(t_t);
	}
	return this;
}
bool c_List2::p_Equals6(Float t_lhs,Float t_rhs){
	return t_lhs==t_rhs;
}
int c_List2::p_Compare5(Float t_lhs,Float t_rhs){
	bbError(String(L"Unable to compare items",23));
	return 0;
}
int c_List2::p_Count(){
	int t_n=0;
	c_Node2* t_node=m__head->m__succ;
	while(t_node!=m__head){
		t_node=t_node->m__succ;
		t_n+=1;
	}
	return t_n;
}
c_Enumerator6* c_List2::p_ObjectEnumerator(){
	return (new c_Enumerator6)->m_new(this);
}
Array<Float > c_List2::p_ToArray(){
	Array<Float > t_arr=Array<Float >(p_Count());
	int t_i=0;
	c_Enumerator6* t_=this->p_ObjectEnumerator();
	while(t_->p_HasNext()){
		Float t_t=t_->p_NextObject();
		t_arr[t_i]=t_t;
		t_i+=1;
	}
	return t_arr;
}
int c_List2::p_Clear(){
	gc_assign(m__head->m__succ,m__head);
	gc_assign(m__head->m__pred,m__head);
	return 0;
}
bool c_List2::p_IsEmpty(){
	return m__head->m__succ==m__head;
}
bool c_List2::p_Contains2(Float t_value){
	c_Node2* t_node=m__head->m__succ;
	while(t_node!=m__head){
		if(p_Equals6(t_node->m__data,t_value)){
			return true;
		}
		t_node=t_node->m__succ;
	}
	return false;
}
c_Node2* c_List2::p_FirstNode(){
	if(m__head->m__succ!=m__head){
		return m__head->m__succ;
	}
	return 0;
}
c_Node2* c_List2::p_LastNode(){
	if(m__head->m__pred!=m__head){
		return m__head->m__pred;
	}
	return 0;
}
Float c_List2::p_First(){
	return m__head->m__succ->m__data;
}
Float c_List2::p_Last(){
	return m__head->m__pred->m__data;
}
Float c_List2::p_RemoveFirst(){
	Float t_data=m__head->m__succ->m__data;
	m__head->m__succ->p_Remove2();
	return t_data;
}
Float c_List2::p_RemoveLast(){
	Float t_data=m__head->m__pred->m__data;
	m__head->m__pred->p_Remove2();
	return t_data;
}
c_Node2* c_List2::p_AddFirst2(Float t_data){
	return (new c_Node2)->m_new(m__head->m__succ,m__head,t_data);
}
c_Node2* c_List2::p_Find3(Float t_value,c_Node2* t_start){
	while(t_start!=m__head){
		if(p_Equals6(t_value,t_start->m__data)){
			return t_start;
		}
		t_start=t_start->m__succ;
	}
	return 0;
}
c_Node2* c_List2::p_Find4(Float t_value){
	return p_Find3(t_value,m__head->m__succ);
}
c_Node2* c_List2::p_FindLast3(Float t_value,c_Node2* t_start){
	while(t_start!=m__head){
		if(p_Equals6(t_value,t_start->m__data)){
			return t_start;
		}
		t_start=t_start->m__pred;
	}
	return 0;
}
c_Node2* c_List2::p_FindLast4(Float t_value){
	return p_FindLast3(t_value,m__head->m__pred);
}
int c_List2::p_RemoveEach2(Float t_value){
	c_Node2* t_node=m__head->m__succ;
	while(t_node!=m__head){
		c_Node2* t_succ=t_node->m__succ;
		if(p_Equals6(t_node->m__data,t_value)){
			t_node->p_Remove2();
		}
		t_node=t_succ;
	}
	return 0;
}
void c_List2::p_Remove3(Float t_value){
	p_RemoveEach2(t_value);
}
void c_List2::p_RemoveFirst3(Float t_value){
	c_Node2* t_node=p_Find4(t_value);
	if((t_node)!=0){
		t_node->p_Remove2();
	}
}
void c_List2::p_RemoveLast3(Float t_value){
	c_Node2* t_node=p_FindLast4(t_value);
	if((t_node)!=0){
		t_node->p_Remove2();
	}
}
c_Node2* c_List2::p_InsertBefore2(Float t_where,Float t_data){
	c_Node2* t_node=p_Find4(t_where);
	if((t_node)!=0){
		return (new c_Node2)->m_new(t_node,t_node->m__pred,t_data);
	}
	return 0;
}
c_Node2* c_List2::p_InsertAfter2(Float t_where,Float t_data){
	c_Node2* t_node=p_Find4(t_where);
	if((t_node)!=0){
		return (new c_Node2)->m_new(t_node->m__succ,t_node,t_data);
	}
	return 0;
}
void c_List2::p_InsertBeforeEach2(Float t_where,Float t_data){
	c_Node2* t_node=p_Find4(t_where);
	while((t_node)!=0){
		(new c_Node2)->m_new(t_node,t_node->m__pred,t_data);
		t_node=p_Find3(t_where,t_node->m__succ);
	}
}
void c_List2::p_InsertAfterEach2(Float t_where,Float t_data){
	c_Node2* t_node=p_Find4(t_where);
	while((t_node)!=0){
		t_node=(new c_Node2)->m_new(t_node->m__succ,t_node,t_data);
		t_node=p_Find3(t_where,t_node->m__succ);
	}
}
c_BackwardsList2* c_List2::p_Backwards(){
	return (new c_BackwardsList2)->m_new(this);
}
int c_List2::p_Sort(int t_ascending){
	int t_ccsgn=-1;
	if((t_ascending)!=0){
		t_ccsgn=1;
	}
	int t_insize=1;
	do{
		int t_merges=0;
		c_Node2* t_tail=m__head;
		c_Node2* t_p=m__head->m__succ;
		while(t_p!=m__head){
			t_merges+=1;
			c_Node2* t_q=t_p->m__succ;
			int t_qsize=t_insize;
			int t_psize=1;
			while(t_psize<t_insize && t_q!=m__head){
				t_psize+=1;
				t_q=t_q->m__succ;
			}
			do{
				c_Node2* t_t=0;
				if(((t_psize)!=0) && ((t_qsize)!=0) && t_q!=m__head){
					int t_cc=p_Compare5(t_p->m__data,t_q->m__data)*t_ccsgn;
					if(t_cc<=0){
						t_t=t_p;
						t_p=t_p->m__succ;
						t_psize-=1;
					}else{
						t_t=t_q;
						t_q=t_q->m__succ;
						t_qsize-=1;
					}
				}else{
					if((t_psize)!=0){
						t_t=t_p;
						t_p=t_p->m__succ;
						t_psize-=1;
					}else{
						if(((t_qsize)!=0) && t_q!=m__head){
							t_t=t_q;
							t_q=t_q->m__succ;
							t_qsize-=1;
						}else{
							break;
						}
					}
				}
				gc_assign(t_t->m__pred,t_tail);
				gc_assign(t_tail->m__succ,t_t);
				t_tail=t_t;
			}while(!(false));
			t_p=t_q;
		}
		gc_assign(t_tail->m__succ,m__head);
		gc_assign(m__head->m__pred,t_tail);
		if(t_merges<=1){
			return 0;
		}
		t_insize*=2;
	}while(!(false));
}
void c_List2::mark(){
	Object::mark();
	gc_mark_q(m__head);
}
c_FloatList::c_FloatList(){
}
c_FloatList* c_FloatList::m_new(Array<Float > t_data){
	c_List2::m_new2(t_data);
	return this;
}
bool c_FloatList::p_Equals6(Float t_lhs,Float t_rhs){
	return t_lhs==t_rhs;
}
int c_FloatList::p_Compare5(Float t_lhs,Float t_rhs){
	if(t_lhs<t_rhs){
		return -1;
	}
	return ((t_lhs>t_rhs)?1:0);
}
c_FloatList* c_FloatList::m_new2(){
	c_List2::m_new();
	return this;
}
void c_FloatList::mark(){
	c_List2::mark();
}
c_Node2::c_Node2(){
	m__succ=0;
	m__pred=0;
	m__data=0;
}
c_Node2* c_Node2::m_new(c_Node2* t_succ,c_Node2* t_pred,Float t_data){
	gc_assign(m__succ,t_succ);
	gc_assign(m__pred,t_pred);
	gc_assign(m__succ->m__pred,this);
	gc_assign(m__pred->m__succ,this);
	m__data=t_data;
	return this;
}
c_Node2* c_Node2::m_new2(){
	return this;
}
int c_Node2::p_Remove2(){
	gc_assign(m__succ->m__pred,m__pred);
	gc_assign(m__pred->m__succ,m__succ);
	return 0;
}
Float c_Node2::p_Value(){
	return m__data;
}
c_Node2* c_Node2::p_GetNode(){
	return this;
}
c_Node2* c_Node2::p_NextNode(){
	return m__succ->p_GetNode();
}
c_Node2* c_Node2::p_PrevNode(){
	return m__pred->p_GetNode();
}
void c_Node2::mark(){
	Object::mark();
	gc_mark_q(m__succ);
	gc_mark_q(m__pred);
}
c_HeadNode2::c_HeadNode2(){
}
c_HeadNode2* c_HeadNode2::m_new(){
	c_Node2::m_new2();
	gc_assign(m__succ,(this));
	gc_assign(m__pred,(this));
	return this;
}
c_Node2* c_HeadNode2::p_GetNode(){
	return 0;
}
void c_HeadNode2::mark(){
	c_Node2::mark();
}
c_List3::c_List3(){
	m__head=((new c_HeadNode3)->m_new());
}
c_List3* c_List3::m_new(){
	return this;
}
c_Node3* c_List3::p_AddLast3(String t_data){
	return (new c_Node3)->m_new(m__head,m__head->m__pred,t_data);
}
c_List3* c_List3::m_new2(Array<String > t_data){
	Array<String > t_=t_data;
	int t_2=0;
	while(t_2<t_.Length()){
		String t_t=t_[t_2];
		t_2=t_2+1;
		p_AddLast3(t_t);
	}
	return this;
}
int c_List3::p_Count(){
	int t_n=0;
	c_Node3* t_node=m__head->m__succ;
	while(t_node!=m__head){
		t_node=t_node->m__succ;
		t_n+=1;
	}
	return t_n;
}
c_Enumerator* c_List3::p_ObjectEnumerator(){
	return (new c_Enumerator)->m_new(this);
}
Array<String > c_List3::p_ToArray(){
	Array<String > t_arr=Array<String >(p_Count());
	int t_i=0;
	c_Enumerator* t_=this->p_ObjectEnumerator();
	while(t_->p_HasNext()){
		String t_t=t_->p_NextObject();
		t_arr[t_i]=t_t;
		t_i+=1;
	}
	return t_arr;
}
bool c_List3::p_Equals7(String t_lhs,String t_rhs){
	return t_lhs==t_rhs;
}
int c_List3::p_Compare6(String t_lhs,String t_rhs){
	bbError(String(L"Unable to compare items",23));
	return 0;
}
int c_List3::p_Clear(){
	gc_assign(m__head->m__succ,m__head);
	gc_assign(m__head->m__pred,m__head);
	return 0;
}
bool c_List3::p_IsEmpty(){
	return m__head->m__succ==m__head;
}
bool c_List3::p_Contains3(String t_value){
	c_Node3* t_node=m__head->m__succ;
	while(t_node!=m__head){
		if(p_Equals7(t_node->m__data,t_value)){
			return true;
		}
		t_node=t_node->m__succ;
	}
	return false;
}
c_Node3* c_List3::p_FirstNode(){
	if(m__head->m__succ!=m__head){
		return m__head->m__succ;
	}
	return 0;
}
c_Node3* c_List3::p_LastNode(){
	if(m__head->m__pred!=m__head){
		return m__head->m__pred;
	}
	return 0;
}
String c_List3::p_First(){
	return m__head->m__succ->m__data;
}
String c_List3::p_Last(){
	return m__head->m__pred->m__data;
}
String c_List3::p_RemoveFirst(){
	String t_data=m__head->m__succ->m__data;
	m__head->m__succ->p_Remove2();
	return t_data;
}
String c_List3::p_RemoveLast(){
	String t_data=m__head->m__pred->m__data;
	m__head->m__pred->p_Remove2();
	return t_data;
}
c_Node3* c_List3::p_AddFirst3(String t_data){
	return (new c_Node3)->m_new(m__head->m__succ,m__head,t_data);
}
c_Node3* c_List3::p_Find5(String t_value,c_Node3* t_start){
	while(t_start!=m__head){
		if(p_Equals7(t_value,t_start->m__data)){
			return t_start;
		}
		t_start=t_start->m__succ;
	}
	return 0;
}
c_Node3* c_List3::p_Find6(String t_value){
	return p_Find5(t_value,m__head->m__succ);
}
c_Node3* c_List3::p_FindLast5(String t_value,c_Node3* t_start){
	while(t_start!=m__head){
		if(p_Equals7(t_value,t_start->m__data)){
			return t_start;
		}
		t_start=t_start->m__pred;
	}
	return 0;
}
c_Node3* c_List3::p_FindLast6(String t_value){
	return p_FindLast5(t_value,m__head->m__pred);
}
int c_List3::p_RemoveEach3(String t_value){
	c_Node3* t_node=m__head->m__succ;
	while(t_node!=m__head){
		c_Node3* t_succ=t_node->m__succ;
		if(p_Equals7(t_node->m__data,t_value)){
			t_node->p_Remove2();
		}
		t_node=t_succ;
	}
	return 0;
}
void c_List3::p_Remove4(String t_value){
	p_RemoveEach3(t_value);
}
void c_List3::p_RemoveFirst4(String t_value){
	c_Node3* t_node=p_Find6(t_value);
	if((t_node)!=0){
		t_node->p_Remove2();
	}
}
void c_List3::p_RemoveLast4(String t_value){
	c_Node3* t_node=p_FindLast6(t_value);
	if((t_node)!=0){
		t_node->p_Remove2();
	}
}
c_Node3* c_List3::p_InsertBefore3(String t_where,String t_data){
	c_Node3* t_node=p_Find6(t_where);
	if((t_node)!=0){
		return (new c_Node3)->m_new(t_node,t_node->m__pred,t_data);
	}
	return 0;
}
c_Node3* c_List3::p_InsertAfter3(String t_where,String t_data){
	c_Node3* t_node=p_Find6(t_where);
	if((t_node)!=0){
		return (new c_Node3)->m_new(t_node->m__succ,t_node,t_data);
	}
	return 0;
}
void c_List3::p_InsertBeforeEach3(String t_where,String t_data){
	c_Node3* t_node=p_Find6(t_where);
	while((t_node)!=0){
		(new c_Node3)->m_new(t_node,t_node->m__pred,t_data);
		t_node=p_Find5(t_where,t_node->m__succ);
	}
}
void c_List3::p_InsertAfterEach3(String t_where,String t_data){
	c_Node3* t_node=p_Find6(t_where);
	while((t_node)!=0){
		t_node=(new c_Node3)->m_new(t_node->m__succ,t_node,t_data);
		t_node=p_Find5(t_where,t_node->m__succ);
	}
}
c_BackwardsList3* c_List3::p_Backwards(){
	return (new c_BackwardsList3)->m_new(this);
}
int c_List3::p_Sort(int t_ascending){
	int t_ccsgn=-1;
	if((t_ascending)!=0){
		t_ccsgn=1;
	}
	int t_insize=1;
	do{
		int t_merges=0;
		c_Node3* t_tail=m__head;
		c_Node3* t_p=m__head->m__succ;
		while(t_p!=m__head){
			t_merges+=1;
			c_Node3* t_q=t_p->m__succ;
			int t_qsize=t_insize;
			int t_psize=1;
			while(t_psize<t_insize && t_q!=m__head){
				t_psize+=1;
				t_q=t_q->m__succ;
			}
			do{
				c_Node3* t_t=0;
				if(((t_psize)!=0) && ((t_qsize)!=0) && t_q!=m__head){
					int t_cc=p_Compare6(t_p->m__data,t_q->m__data)*t_ccsgn;
					if(t_cc<=0){
						t_t=t_p;
						t_p=t_p->m__succ;
						t_psize-=1;
					}else{
						t_t=t_q;
						t_q=t_q->m__succ;
						t_qsize-=1;
					}
				}else{
					if((t_psize)!=0){
						t_t=t_p;
						t_p=t_p->m__succ;
						t_psize-=1;
					}else{
						if(((t_qsize)!=0) && t_q!=m__head){
							t_t=t_q;
							t_q=t_q->m__succ;
							t_qsize-=1;
						}else{
							break;
						}
					}
				}
				gc_assign(t_t->m__pred,t_tail);
				gc_assign(t_tail->m__succ,t_t);
				t_tail=t_t;
			}while(!(false));
			t_p=t_q;
		}
		gc_assign(t_tail->m__succ,m__head);
		gc_assign(m__head->m__pred,t_tail);
		if(t_merges<=1){
			return 0;
		}
		t_insize*=2;
	}while(!(false));
}
void c_List3::mark(){
	Object::mark();
	gc_mark_q(m__head);
}
c_StringList::c_StringList(){
}
c_StringList* c_StringList::m_new(Array<String > t_data){
	c_List3::m_new2(t_data);
	return this;
}
String c_StringList::p_Join(String t_separator){
	return t_separator.Join(p_ToArray());
}
bool c_StringList::p_Equals7(String t_lhs,String t_rhs){
	return t_lhs==t_rhs;
}
int c_StringList::p_Compare6(String t_lhs,String t_rhs){
	return t_lhs.Compare(t_rhs);
}
c_StringList* c_StringList::m_new2(){
	c_List3::m_new();
	return this;
}
void c_StringList::mark(){
	c_List3::mark();
}
c_Node3::c_Node3(){
	m__succ=0;
	m__pred=0;
	m__data=String();
}
c_Node3* c_Node3::m_new(c_Node3* t_succ,c_Node3* t_pred,String t_data){
	gc_assign(m__succ,t_succ);
	gc_assign(m__pred,t_pred);
	gc_assign(m__succ->m__pred,this);
	gc_assign(m__pred->m__succ,this);
	m__data=t_data;
	return this;
}
c_Node3* c_Node3::m_new2(){
	return this;
}
int c_Node3::p_Remove2(){
	gc_assign(m__succ->m__pred,m__pred);
	gc_assign(m__pred->m__succ,m__succ);
	return 0;
}
String c_Node3::p_Value(){
	return m__data;
}
c_Node3* c_Node3::p_GetNode(){
	return this;
}
c_Node3* c_Node3::p_NextNode(){
	return m__succ->p_GetNode();
}
c_Node3* c_Node3::p_PrevNode(){
	return m__pred->p_GetNode();
}
void c_Node3::mark(){
	Object::mark();
	gc_mark_q(m__succ);
	gc_mark_q(m__pred);
}
c_HeadNode3::c_HeadNode3(){
}
c_HeadNode3* c_HeadNode3::m_new(){
	c_Node3::m_new2();
	gc_assign(m__succ,(this));
	gc_assign(m__pred,(this));
	return this;
}
c_Node3* c_HeadNode3::p_GetNode(){
	return 0;
}
void c_HeadNode3::mark(){
	c_Node3::mark();
}
c_Enumerator::c_Enumerator(){
	m__list=0;
	m__curr=0;
}
c_Enumerator* c_Enumerator::m_new(c_List3* t_list){
	gc_assign(m__list,t_list);
	gc_assign(m__curr,t_list->m__head->m__succ);
	return this;
}
c_Enumerator* c_Enumerator::m_new2(){
	return this;
}
bool c_Enumerator::p_HasNext(){
	while(m__curr->m__succ->m__pred!=m__curr){
		gc_assign(m__curr,m__curr->m__succ);
	}
	return m__curr!=m__list->m__head;
}
String c_Enumerator::p_NextObject(){
	String t_data=m__curr->m__data;
	gc_assign(m__curr,m__curr->m__succ);
	return t_data;
}
void c_Enumerator::mark(){
	Object::mark();
	gc_mark_q(m__list);
	gc_mark_q(m__curr);
}
int bb_math_Sgn(int t_x){
	if(t_x<0){
		return -1;
	}
	return ((t_x>0)?1:0);
}
int bb_math_Abs(int t_x){
	if(t_x>=0){
		return t_x;
	}
	return -t_x;
}
int bb_math_Min(int t_x,int t_y){
	if(t_x<t_y){
		return t_x;
	}
	return t_y;
}
int bb_math_Max(int t_x,int t_y){
	if(t_x>t_y){
		return t_x;
	}
	return t_y;
}
int bb_math_Clamp(int t_n,int t_min,int t_max){
	if(t_n<t_min){
		return t_min;
	}
	if(t_n>t_max){
		return t_max;
	}
	return t_n;
}
Float bb_math_Sgn2(Float t_x){
	if(t_x<FLOAT(0.0)){
		return FLOAT(-1.0);
	}
	if(t_x>FLOAT(0.0)){
		return FLOAT(1.0);
	}
	return FLOAT(0.0);
}
Float bb_math_Abs2(Float t_x){
	if(t_x>=FLOAT(0.0)){
		return t_x;
	}
	return -t_x;
}
Float bb_math_Min2(Float t_x,Float t_y){
	if(t_x<t_y){
		return t_x;
	}
	return t_y;
}
Float bb_math_Max2(Float t_x,Float t_y){
	if(t_x>t_y){
		return t_x;
	}
	return t_y;
}
Float bb_math_Clamp2(Float t_n,Float t_min,Float t_max){
	if(t_n<t_min){
		return t_min;
	}
	if(t_n>t_max){
		return t_max;
	}
	return t_n;
}
int bb_random_Seed;
Float bb_random_Rnd(){
	bb_random_Seed=bb_random_Seed*1664525+1013904223|0;
	return Float(bb_random_Seed>>8&16777215)/FLOAT(16777216.0);
}
Float bb_random_Rnd2(Float t_low,Float t_high){
	return bb_random_Rnd3(t_high-t_low)+t_low;
}
Float bb_random_Rnd3(Float t_range){
	return bb_random_Rnd()*t_range;
}
c_Set::c_Set(){
	m_map=0;
}
c_Set* c_Set::m_new(c_Map* t_map){
	gc_assign(this->m_map,t_map);
	return this;
}
c_Set* c_Set::m_new2(){
	return this;
}
int c_Set::p_Clear(){
	m_map->p_Clear();
	return 0;
}
int c_Set::p_Count(){
	return m_map->p_Count();
}
bool c_Set::p_IsEmpty(){
	return m_map->p_IsEmpty();
}
bool c_Set::p_Contains(int t_value){
	return m_map->p_Contains(t_value);
}
int c_Set::p_Insert(int t_value){
	m_map->p_Insert2(t_value,0);
	return 0;
}
int c_Set::p_Remove(int t_value){
	m_map->p_Remove(t_value);
	return 0;
}
c_KeyEnumerator2* c_Set::p_ObjectEnumerator(){
	return m_map->p_Keys()->p_ObjectEnumerator();
}
void c_Set::mark(){
	Object::mark();
	gc_mark_q(m_map);
}
c_IntSet::c_IntSet(){
}
c_IntSet* c_IntSet::m_new(){
	c_Set::m_new((new c_IntMap)->m_new());
	return this;
}
void c_IntSet::mark(){
	c_Set::mark();
}
c_Map::c_Map(){
	m_root=0;
}
c_Map* c_Map::m_new(){
	return this;
}
int c_Map::p_Clear(){
	m_root=0;
	return 0;
}
int c_Map::p_Count(){
	if((m_root)!=0){
		return m_root->p_Count2(0);
	}
	return 0;
}
bool c_Map::p_IsEmpty(){
	return m_root==0;
}
c_Node6* c_Map::p_FindNode(int t_key){
	c_Node6* t_node=m_root;
	while((t_node)!=0){
		int t_cmp=p_Compare4(t_key,t_node->m_key);
		if(t_cmp>0){
			t_node=t_node->m_right;
		}else{
			if(t_cmp<0){
				t_node=t_node->m_left;
			}else{
				return t_node;
			}
		}
	}
	return t_node;
}
bool c_Map::p_Contains(int t_key){
	return p_FindNode(t_key)!=0;
}
int c_Map::p_RotateLeft(c_Node6* t_node){
	c_Node6* t_child=t_node->m_right;
	gc_assign(t_node->m_right,t_child->m_left);
	if((t_child->m_left)!=0){
		gc_assign(t_child->m_left->m_parent,t_node);
	}
	gc_assign(t_child->m_parent,t_node->m_parent);
	if((t_node->m_parent)!=0){
		if(t_node==t_node->m_parent->m_left){
			gc_assign(t_node->m_parent->m_left,t_child);
		}else{
			gc_assign(t_node->m_parent->m_right,t_child);
		}
	}else{
		gc_assign(m_root,t_child);
	}
	gc_assign(t_child->m_left,t_node);
	gc_assign(t_node->m_parent,t_child);
	return 0;
}
int c_Map::p_RotateRight(c_Node6* t_node){
	c_Node6* t_child=t_node->m_left;
	gc_assign(t_node->m_left,t_child->m_right);
	if((t_child->m_right)!=0){
		gc_assign(t_child->m_right->m_parent,t_node);
	}
	gc_assign(t_child->m_parent,t_node->m_parent);
	if((t_node->m_parent)!=0){
		if(t_node==t_node->m_parent->m_right){
			gc_assign(t_node->m_parent->m_right,t_child);
		}else{
			gc_assign(t_node->m_parent->m_left,t_child);
		}
	}else{
		gc_assign(m_root,t_child);
	}
	gc_assign(t_child->m_right,t_node);
	gc_assign(t_node->m_parent,t_child);
	return 0;
}
int c_Map::p_InsertFixup(c_Node6* t_node){
	while(((t_node->m_parent)!=0) && t_node->m_parent->m_color==-1 && ((t_node->m_parent->m_parent)!=0)){
		if(t_node->m_parent==t_node->m_parent->m_parent->m_left){
			c_Node6* t_uncle=t_node->m_parent->m_parent->m_right;
			if(((t_uncle)!=0) && t_uncle->m_color==-1){
				t_node->m_parent->m_color=1;
				t_uncle->m_color=1;
				t_uncle->m_parent->m_color=-1;
				t_node=t_uncle->m_parent;
			}else{
				if(t_node==t_node->m_parent->m_right){
					t_node=t_node->m_parent;
					p_RotateLeft(t_node);
				}
				t_node->m_parent->m_color=1;
				t_node->m_parent->m_parent->m_color=-1;
				p_RotateRight(t_node->m_parent->m_parent);
			}
		}else{
			c_Node6* t_uncle2=t_node->m_parent->m_parent->m_left;
			if(((t_uncle2)!=0) && t_uncle2->m_color==-1){
				t_node->m_parent->m_color=1;
				t_uncle2->m_color=1;
				t_uncle2->m_parent->m_color=-1;
				t_node=t_uncle2->m_parent;
			}else{
				if(t_node==t_node->m_parent->m_left){
					t_node=t_node->m_parent;
					p_RotateRight(t_node);
				}
				t_node->m_parent->m_color=1;
				t_node->m_parent->m_parent->m_color=-1;
				p_RotateLeft(t_node->m_parent->m_parent);
			}
		}
	}
	m_root->m_color=1;
	return 0;
}
bool c_Map::p_Set4(int t_key,Object* t_value){
	c_Node6* t_node=m_root;
	c_Node6* t_parent=0;
	int t_cmp=0;
	while((t_node)!=0){
		t_parent=t_node;
		t_cmp=p_Compare4(t_key,t_node->m_key);
		if(t_cmp>0){
			t_node=t_node->m_right;
		}else{
			if(t_cmp<0){
				t_node=t_node->m_left;
			}else{
				gc_assign(t_node->m_value,t_value);
				return false;
			}
		}
	}
	t_node=(new c_Node6)->m_new(t_key,t_value,-1,t_parent);
	if((t_parent)!=0){
		if(t_cmp>0){
			gc_assign(t_parent->m_right,t_node);
		}else{
			gc_assign(t_parent->m_left,t_node);
		}
		p_InsertFixup(t_node);
	}else{
		gc_assign(m_root,t_node);
	}
	return true;
}
bool c_Map::p_Add(int t_key,Object* t_value){
	c_Node6* t_node=m_root;
	c_Node6* t_parent=0;
	int t_cmp=0;
	while((t_node)!=0){
		t_parent=t_node;
		t_cmp=p_Compare4(t_key,t_node->m_key);
		if(t_cmp>0){
			t_node=t_node->m_right;
		}else{
			if(t_cmp<0){
				t_node=t_node->m_left;
			}else{
				return false;
			}
		}
	}
	t_node=(new c_Node6)->m_new(t_key,t_value,-1,t_parent);
	if((t_parent)!=0){
		if(t_cmp>0){
			gc_assign(t_parent->m_right,t_node);
		}else{
			gc_assign(t_parent->m_left,t_node);
		}
		p_InsertFixup(t_node);
	}else{
		gc_assign(m_root,t_node);
	}
	return true;
}
bool c_Map::p_Update(int t_key,Object* t_value){
	c_Node6* t_node=p_FindNode(t_key);
	if((t_node)!=0){
		gc_assign(t_node->m_value,t_value);
		return true;
	}
	return false;
}
Object* c_Map::p_Get(int t_key){
	c_Node6* t_node=p_FindNode(t_key);
	if((t_node)!=0){
		return t_node->m_value;
	}
	return 0;
}
int c_Map::p_DeleteFixup(c_Node6* t_node,c_Node6* t_parent){
	while(t_node!=m_root && (!((t_node)!=0) || t_node->m_color==1)){
		if(t_node==t_parent->m_left){
			c_Node6* t_sib=t_parent->m_right;
			if(t_sib->m_color==-1){
				t_sib->m_color=1;
				t_parent->m_color=-1;
				p_RotateLeft(t_parent);
				t_sib=t_parent->m_right;
			}
			if((!((t_sib->m_left)!=0) || t_sib->m_left->m_color==1) && (!((t_sib->m_right)!=0) || t_sib->m_right->m_color==1)){
				t_sib->m_color=-1;
				t_node=t_parent;
				t_parent=t_parent->m_parent;
			}else{
				if(!((t_sib->m_right)!=0) || t_sib->m_right->m_color==1){
					t_sib->m_left->m_color=1;
					t_sib->m_color=-1;
					p_RotateRight(t_sib);
					t_sib=t_parent->m_right;
				}
				t_sib->m_color=t_parent->m_color;
				t_parent->m_color=1;
				t_sib->m_right->m_color=1;
				p_RotateLeft(t_parent);
				t_node=m_root;
			}
		}else{
			c_Node6* t_sib2=t_parent->m_left;
			if(t_sib2->m_color==-1){
				t_sib2->m_color=1;
				t_parent->m_color=-1;
				p_RotateRight(t_parent);
				t_sib2=t_parent->m_left;
			}
			if((!((t_sib2->m_right)!=0) || t_sib2->m_right->m_color==1) && (!((t_sib2->m_left)!=0) || t_sib2->m_left->m_color==1)){
				t_sib2->m_color=-1;
				t_node=t_parent;
				t_parent=t_parent->m_parent;
			}else{
				if(!((t_sib2->m_left)!=0) || t_sib2->m_left->m_color==1){
					t_sib2->m_right->m_color=1;
					t_sib2->m_color=-1;
					p_RotateLeft(t_sib2);
					t_sib2=t_parent->m_left;
				}
				t_sib2->m_color=t_parent->m_color;
				t_parent->m_color=1;
				t_sib2->m_left->m_color=1;
				p_RotateRight(t_parent);
				t_node=m_root;
			}
		}
	}
	if((t_node)!=0){
		t_node->m_color=1;
	}
	return 0;
}
int c_Map::p_RemoveNode(c_Node6* t_node){
	c_Node6* t_splice=0;
	c_Node6* t_child=0;
	if(!((t_node->m_left)!=0)){
		t_splice=t_node;
		t_child=t_node->m_right;
	}else{
		if(!((t_node->m_right)!=0)){
			t_splice=t_node;
			t_child=t_node->m_left;
		}else{
			t_splice=t_node->m_left;
			while((t_splice->m_right)!=0){
				t_splice=t_splice->m_right;
			}
			t_child=t_splice->m_left;
			t_node->m_key=t_splice->m_key;
			gc_assign(t_node->m_value,t_splice->m_value);
		}
	}
	c_Node6* t_parent=t_splice->m_parent;
	if((t_child)!=0){
		gc_assign(t_child->m_parent,t_parent);
	}
	if(!((t_parent)!=0)){
		gc_assign(m_root,t_child);
		return 0;
	}
	if(t_splice==t_parent->m_left){
		gc_assign(t_parent->m_left,t_child);
	}else{
		gc_assign(t_parent->m_right,t_child);
	}
	if(t_splice->m_color==1){
		p_DeleteFixup(t_child,t_parent);
	}
	return 0;
}
int c_Map::p_Remove(int t_key){
	c_Node6* t_node=p_FindNode(t_key);
	if(!((t_node)!=0)){
		return 0;
	}
	p_RemoveNode(t_node);
	return 1;
}
c_MapKeys2* c_Map::p_Keys(){
	return (new c_MapKeys2)->m_new(this);
}
c_MapValues* c_Map::p_Values(){
	return (new c_MapValues)->m_new(this);
}
c_Node6* c_Map::p_FirstNode(){
	if(!((m_root)!=0)){
		return 0;
	}
	c_Node6* t_node=m_root;
	while((t_node->m_left)!=0){
		t_node=t_node->m_left;
	}
	return t_node;
}
c_NodeEnumerator* c_Map::p_ObjectEnumerator(){
	return (new c_NodeEnumerator)->m_new(p_FirstNode());
}
bool c_Map::p_Insert2(int t_key,Object* t_value){
	return p_Set4(t_key,t_value);
}
Object* c_Map::p_ValueForKey(int t_key){
	return p_Get(t_key);
}
c_Node6* c_Map::p_LastNode(){
	if(!((m_root)!=0)){
		return 0;
	}
	c_Node6* t_node=m_root;
	while((t_node->m_right)!=0){
		t_node=t_node->m_right;
	}
	return t_node;
}
void c_Map::mark(){
	Object::mark();
	gc_mark_q(m_root);
}
c_IntMap::c_IntMap(){
}
c_IntMap* c_IntMap::m_new(){
	c_Map::m_new();
	return this;
}
int c_IntMap::p_Compare4(int t_lhs,int t_rhs){
	return t_lhs-t_rhs;
}
void c_IntMap::mark(){
	c_Map::mark();
}
c_Set2::c_Set2(){
	m_map=0;
}
c_Set2* c_Set2::m_new(c_Map2* t_map){
	gc_assign(this->m_map,t_map);
	return this;
}
c_Set2* c_Set2::m_new2(){
	return this;
}
int c_Set2::p_Clear(){
	m_map->p_Clear();
	return 0;
}
int c_Set2::p_Count(){
	return m_map->p_Count();
}
bool c_Set2::p_IsEmpty(){
	return m_map->p_IsEmpty();
}
bool c_Set2::p_Contains2(Float t_value){
	return m_map->p_Contains2(t_value);
}
int c_Set2::p_Insert3(Float t_value){
	m_map->p_Insert4(t_value,0);
	return 0;
}
int c_Set2::p_Remove3(Float t_value){
	m_map->p_Remove3(t_value);
	return 0;
}
c_KeyEnumerator3* c_Set2::p_ObjectEnumerator(){
	return m_map->p_Keys()->p_ObjectEnumerator();
}
void c_Set2::mark(){
	Object::mark();
	gc_mark_q(m_map);
}
c_FloatSet::c_FloatSet(){
}
c_FloatSet* c_FloatSet::m_new(){
	c_Set2::m_new((new c_FloatMap)->m_new());
	return this;
}
void c_FloatSet::mark(){
	c_Set2::mark();
}
c_Map2::c_Map2(){
	m_root=0;
}
c_Map2* c_Map2::m_new(){
	return this;
}
int c_Map2::p_Clear(){
	m_root=0;
	return 0;
}
int c_Map2::p_Count(){
	if((m_root)!=0){
		return m_root->p_Count2(0);
	}
	return 0;
}
bool c_Map2::p_IsEmpty(){
	return m_root==0;
}
c_Node7* c_Map2::p_FindNode2(Float t_key){
	c_Node7* t_node=m_root;
	while((t_node)!=0){
		int t_cmp=p_Compare5(t_key,t_node->m_key);
		if(t_cmp>0){
			t_node=t_node->m_right;
		}else{
			if(t_cmp<0){
				t_node=t_node->m_left;
			}else{
				return t_node;
			}
		}
	}
	return t_node;
}
bool c_Map2::p_Contains2(Float t_key){
	return p_FindNode2(t_key)!=0;
}
int c_Map2::p_RotateLeft2(c_Node7* t_node){
	c_Node7* t_child=t_node->m_right;
	gc_assign(t_node->m_right,t_child->m_left);
	if((t_child->m_left)!=0){
		gc_assign(t_child->m_left->m_parent,t_node);
	}
	gc_assign(t_child->m_parent,t_node->m_parent);
	if((t_node->m_parent)!=0){
		if(t_node==t_node->m_parent->m_left){
			gc_assign(t_node->m_parent->m_left,t_child);
		}else{
			gc_assign(t_node->m_parent->m_right,t_child);
		}
	}else{
		gc_assign(m_root,t_child);
	}
	gc_assign(t_child->m_left,t_node);
	gc_assign(t_node->m_parent,t_child);
	return 0;
}
int c_Map2::p_RotateRight2(c_Node7* t_node){
	c_Node7* t_child=t_node->m_left;
	gc_assign(t_node->m_left,t_child->m_right);
	if((t_child->m_right)!=0){
		gc_assign(t_child->m_right->m_parent,t_node);
	}
	gc_assign(t_child->m_parent,t_node->m_parent);
	if((t_node->m_parent)!=0){
		if(t_node==t_node->m_parent->m_right){
			gc_assign(t_node->m_parent->m_right,t_child);
		}else{
			gc_assign(t_node->m_parent->m_left,t_child);
		}
	}else{
		gc_assign(m_root,t_child);
	}
	gc_assign(t_child->m_right,t_node);
	gc_assign(t_node->m_parent,t_child);
	return 0;
}
int c_Map2::p_InsertFixup2(c_Node7* t_node){
	while(((t_node->m_parent)!=0) && t_node->m_parent->m_color==-1 && ((t_node->m_parent->m_parent)!=0)){
		if(t_node->m_parent==t_node->m_parent->m_parent->m_left){
			c_Node7* t_uncle=t_node->m_parent->m_parent->m_right;
			if(((t_uncle)!=0) && t_uncle->m_color==-1){
				t_node->m_parent->m_color=1;
				t_uncle->m_color=1;
				t_uncle->m_parent->m_color=-1;
				t_node=t_uncle->m_parent;
			}else{
				if(t_node==t_node->m_parent->m_right){
					t_node=t_node->m_parent;
					p_RotateLeft2(t_node);
				}
				t_node->m_parent->m_color=1;
				t_node->m_parent->m_parent->m_color=-1;
				p_RotateRight2(t_node->m_parent->m_parent);
			}
		}else{
			c_Node7* t_uncle2=t_node->m_parent->m_parent->m_left;
			if(((t_uncle2)!=0) && t_uncle2->m_color==-1){
				t_node->m_parent->m_color=1;
				t_uncle2->m_color=1;
				t_uncle2->m_parent->m_color=-1;
				t_node=t_uncle2->m_parent;
			}else{
				if(t_node==t_node->m_parent->m_left){
					t_node=t_node->m_parent;
					p_RotateRight2(t_node);
				}
				t_node->m_parent->m_color=1;
				t_node->m_parent->m_parent->m_color=-1;
				p_RotateLeft2(t_node->m_parent->m_parent);
			}
		}
	}
	m_root->m_color=1;
	return 0;
}
bool c_Map2::p_Set5(Float t_key,Object* t_value){
	c_Node7* t_node=m_root;
	c_Node7* t_parent=0;
	int t_cmp=0;
	while((t_node)!=0){
		t_parent=t_node;
		t_cmp=p_Compare5(t_key,t_node->m_key);
		if(t_cmp>0){
			t_node=t_node->m_right;
		}else{
			if(t_cmp<0){
				t_node=t_node->m_left;
			}else{
				gc_assign(t_node->m_value,t_value);
				return false;
			}
		}
	}
	t_node=(new c_Node7)->m_new(t_key,t_value,-1,t_parent);
	if((t_parent)!=0){
		if(t_cmp>0){
			gc_assign(t_parent->m_right,t_node);
		}else{
			gc_assign(t_parent->m_left,t_node);
		}
		p_InsertFixup2(t_node);
	}else{
		gc_assign(m_root,t_node);
	}
	return true;
}
bool c_Map2::p_Add2(Float t_key,Object* t_value){
	c_Node7* t_node=m_root;
	c_Node7* t_parent=0;
	int t_cmp=0;
	while((t_node)!=0){
		t_parent=t_node;
		t_cmp=p_Compare5(t_key,t_node->m_key);
		if(t_cmp>0){
			t_node=t_node->m_right;
		}else{
			if(t_cmp<0){
				t_node=t_node->m_left;
			}else{
				return false;
			}
		}
	}
	t_node=(new c_Node7)->m_new(t_key,t_value,-1,t_parent);
	if((t_parent)!=0){
		if(t_cmp>0){
			gc_assign(t_parent->m_right,t_node);
		}else{
			gc_assign(t_parent->m_left,t_node);
		}
		p_InsertFixup2(t_node);
	}else{
		gc_assign(m_root,t_node);
	}
	return true;
}
bool c_Map2::p_Update2(Float t_key,Object* t_value){
	c_Node7* t_node=p_FindNode2(t_key);
	if((t_node)!=0){
		gc_assign(t_node->m_value,t_value);
		return true;
	}
	return false;
}
Object* c_Map2::p_Get2(Float t_key){
	c_Node7* t_node=p_FindNode2(t_key);
	if((t_node)!=0){
		return t_node->m_value;
	}
	return 0;
}
int c_Map2::p_DeleteFixup2(c_Node7* t_node,c_Node7* t_parent){
	while(t_node!=m_root && (!((t_node)!=0) || t_node->m_color==1)){
		if(t_node==t_parent->m_left){
			c_Node7* t_sib=t_parent->m_right;
			if(t_sib->m_color==-1){
				t_sib->m_color=1;
				t_parent->m_color=-1;
				p_RotateLeft2(t_parent);
				t_sib=t_parent->m_right;
			}
			if((!((t_sib->m_left)!=0) || t_sib->m_left->m_color==1) && (!((t_sib->m_right)!=0) || t_sib->m_right->m_color==1)){
				t_sib->m_color=-1;
				t_node=t_parent;
				t_parent=t_parent->m_parent;
			}else{
				if(!((t_sib->m_right)!=0) || t_sib->m_right->m_color==1){
					t_sib->m_left->m_color=1;
					t_sib->m_color=-1;
					p_RotateRight2(t_sib);
					t_sib=t_parent->m_right;
				}
				t_sib->m_color=t_parent->m_color;
				t_parent->m_color=1;
				t_sib->m_right->m_color=1;
				p_RotateLeft2(t_parent);
				t_node=m_root;
			}
		}else{
			c_Node7* t_sib2=t_parent->m_left;
			if(t_sib2->m_color==-1){
				t_sib2->m_color=1;
				t_parent->m_color=-1;
				p_RotateRight2(t_parent);
				t_sib2=t_parent->m_left;
			}
			if((!((t_sib2->m_right)!=0) || t_sib2->m_right->m_color==1) && (!((t_sib2->m_left)!=0) || t_sib2->m_left->m_color==1)){
				t_sib2->m_color=-1;
				t_node=t_parent;
				t_parent=t_parent->m_parent;
			}else{
				if(!((t_sib2->m_left)!=0) || t_sib2->m_left->m_color==1){
					t_sib2->m_right->m_color=1;
					t_sib2->m_color=-1;
					p_RotateLeft2(t_sib2);
					t_sib2=t_parent->m_left;
				}
				t_sib2->m_color=t_parent->m_color;
				t_parent->m_color=1;
				t_sib2->m_left->m_color=1;
				p_RotateRight2(t_parent);
				t_node=m_root;
			}
		}
	}
	if((t_node)!=0){
		t_node->m_color=1;
	}
	return 0;
}
int c_Map2::p_RemoveNode2(c_Node7* t_node){
	c_Node7* t_splice=0;
	c_Node7* t_child=0;
	if(!((t_node->m_left)!=0)){
		t_splice=t_node;
		t_child=t_node->m_right;
	}else{
		if(!((t_node->m_right)!=0)){
			t_splice=t_node;
			t_child=t_node->m_left;
		}else{
			t_splice=t_node->m_left;
			while((t_splice->m_right)!=0){
				t_splice=t_splice->m_right;
			}
			t_child=t_splice->m_left;
			t_node->m_key=t_splice->m_key;
			gc_assign(t_node->m_value,t_splice->m_value);
		}
	}
	c_Node7* t_parent=t_splice->m_parent;
	if((t_child)!=0){
		gc_assign(t_child->m_parent,t_parent);
	}
	if(!((t_parent)!=0)){
		gc_assign(m_root,t_child);
		return 0;
	}
	if(t_splice==t_parent->m_left){
		gc_assign(t_parent->m_left,t_child);
	}else{
		gc_assign(t_parent->m_right,t_child);
	}
	if(t_splice->m_color==1){
		p_DeleteFixup2(t_child,t_parent);
	}
	return 0;
}
int c_Map2::p_Remove3(Float t_key){
	c_Node7* t_node=p_FindNode2(t_key);
	if(!((t_node)!=0)){
		return 0;
	}
	p_RemoveNode2(t_node);
	return 1;
}
c_MapKeys3* c_Map2::p_Keys(){
	return (new c_MapKeys3)->m_new(this);
}
c_MapValues2* c_Map2::p_Values(){
	return (new c_MapValues2)->m_new(this);
}
c_Node7* c_Map2::p_FirstNode(){
	if(!((m_root)!=0)){
		return 0;
	}
	c_Node7* t_node=m_root;
	while((t_node->m_left)!=0){
		t_node=t_node->m_left;
	}
	return t_node;
}
c_NodeEnumerator2* c_Map2::p_ObjectEnumerator(){
	return (new c_NodeEnumerator2)->m_new(p_FirstNode());
}
bool c_Map2::p_Insert4(Float t_key,Object* t_value){
	return p_Set5(t_key,t_value);
}
Object* c_Map2::p_ValueForKey2(Float t_key){
	return p_Get2(t_key);
}
c_Node7* c_Map2::p_LastNode(){
	if(!((m_root)!=0)){
		return 0;
	}
	c_Node7* t_node=m_root;
	while((t_node->m_right)!=0){
		t_node=t_node->m_right;
	}
	return t_node;
}
void c_Map2::mark(){
	Object::mark();
	gc_mark_q(m_root);
}
c_FloatMap::c_FloatMap(){
}
c_FloatMap* c_FloatMap::m_new(){
	c_Map2::m_new();
	return this;
}
int c_FloatMap::p_Compare5(Float t_lhs,Float t_rhs){
	if(t_lhs<t_rhs){
		return -1;
	}
	return ((t_lhs>t_rhs)?1:0);
}
void c_FloatMap::mark(){
	c_Map2::mark();
}
c_Set3::c_Set3(){
	m_map=0;
}
c_Set3* c_Set3::m_new(c_Map3* t_map){
	gc_assign(this->m_map,t_map);
	return this;
}
c_Set3* c_Set3::m_new2(){
	return this;
}
int c_Set3::p_Clear(){
	m_map->p_Clear();
	return 0;
}
int c_Set3::p_Count(){
	return m_map->p_Count();
}
bool c_Set3::p_IsEmpty(){
	return m_map->p_IsEmpty();
}
bool c_Set3::p_Contains3(String t_value){
	return m_map->p_Contains3(t_value);
}
int c_Set3::p_Insert5(String t_value){
	m_map->p_Insert6(t_value,0);
	return 0;
}
int c_Set3::p_Remove4(String t_value){
	m_map->p_Remove4(t_value);
	return 0;
}
c_KeyEnumerator4* c_Set3::p_ObjectEnumerator(){
	return m_map->p_Keys()->p_ObjectEnumerator();
}
void c_Set3::mark(){
	Object::mark();
	gc_mark_q(m_map);
}
c_StringSet::c_StringSet(){
}
c_StringSet* c_StringSet::m_new(){
	c_Set3::m_new((new c_StringMap)->m_new());
	return this;
}
void c_StringSet::mark(){
	c_Set3::mark();
}
c_Map3::c_Map3(){
	m_root=0;
}
c_Map3* c_Map3::m_new(){
	return this;
}
int c_Map3::p_Clear(){
	m_root=0;
	return 0;
}
int c_Map3::p_Count(){
	if((m_root)!=0){
		return m_root->p_Count2(0);
	}
	return 0;
}
bool c_Map3::p_IsEmpty(){
	return m_root==0;
}
c_Node8* c_Map3::p_FindNode3(String t_key){
	c_Node8* t_node=m_root;
	while((t_node)!=0){
		int t_cmp=p_Compare6(t_key,t_node->m_key);
		if(t_cmp>0){
			t_node=t_node->m_right;
		}else{
			if(t_cmp<0){
				t_node=t_node->m_left;
			}else{
				return t_node;
			}
		}
	}
	return t_node;
}
bool c_Map3::p_Contains3(String t_key){
	return p_FindNode3(t_key)!=0;
}
int c_Map3::p_RotateLeft3(c_Node8* t_node){
	c_Node8* t_child=t_node->m_right;
	gc_assign(t_node->m_right,t_child->m_left);
	if((t_child->m_left)!=0){
		gc_assign(t_child->m_left->m_parent,t_node);
	}
	gc_assign(t_child->m_parent,t_node->m_parent);
	if((t_node->m_parent)!=0){
		if(t_node==t_node->m_parent->m_left){
			gc_assign(t_node->m_parent->m_left,t_child);
		}else{
			gc_assign(t_node->m_parent->m_right,t_child);
		}
	}else{
		gc_assign(m_root,t_child);
	}
	gc_assign(t_child->m_left,t_node);
	gc_assign(t_node->m_parent,t_child);
	return 0;
}
int c_Map3::p_RotateRight3(c_Node8* t_node){
	c_Node8* t_child=t_node->m_left;
	gc_assign(t_node->m_left,t_child->m_right);
	if((t_child->m_right)!=0){
		gc_assign(t_child->m_right->m_parent,t_node);
	}
	gc_assign(t_child->m_parent,t_node->m_parent);
	if((t_node->m_parent)!=0){
		if(t_node==t_node->m_parent->m_right){
			gc_assign(t_node->m_parent->m_right,t_child);
		}else{
			gc_assign(t_node->m_parent->m_left,t_child);
		}
	}else{
		gc_assign(m_root,t_child);
	}
	gc_assign(t_child->m_right,t_node);
	gc_assign(t_node->m_parent,t_child);
	return 0;
}
int c_Map3::p_InsertFixup3(c_Node8* t_node){
	while(((t_node->m_parent)!=0) && t_node->m_parent->m_color==-1 && ((t_node->m_parent->m_parent)!=0)){
		if(t_node->m_parent==t_node->m_parent->m_parent->m_left){
			c_Node8* t_uncle=t_node->m_parent->m_parent->m_right;
			if(((t_uncle)!=0) && t_uncle->m_color==-1){
				t_node->m_parent->m_color=1;
				t_uncle->m_color=1;
				t_uncle->m_parent->m_color=-1;
				t_node=t_uncle->m_parent;
			}else{
				if(t_node==t_node->m_parent->m_right){
					t_node=t_node->m_parent;
					p_RotateLeft3(t_node);
				}
				t_node->m_parent->m_color=1;
				t_node->m_parent->m_parent->m_color=-1;
				p_RotateRight3(t_node->m_parent->m_parent);
			}
		}else{
			c_Node8* t_uncle2=t_node->m_parent->m_parent->m_left;
			if(((t_uncle2)!=0) && t_uncle2->m_color==-1){
				t_node->m_parent->m_color=1;
				t_uncle2->m_color=1;
				t_uncle2->m_parent->m_color=-1;
				t_node=t_uncle2->m_parent;
			}else{
				if(t_node==t_node->m_parent->m_left){
					t_node=t_node->m_parent;
					p_RotateRight3(t_node);
				}
				t_node->m_parent->m_color=1;
				t_node->m_parent->m_parent->m_color=-1;
				p_RotateLeft3(t_node->m_parent->m_parent);
			}
		}
	}
	m_root->m_color=1;
	return 0;
}
bool c_Map3::p_Set6(String t_key,Object* t_value){
	c_Node8* t_node=m_root;
	c_Node8* t_parent=0;
	int t_cmp=0;
	while((t_node)!=0){
		t_parent=t_node;
		t_cmp=p_Compare6(t_key,t_node->m_key);
		if(t_cmp>0){
			t_node=t_node->m_right;
		}else{
			if(t_cmp<0){
				t_node=t_node->m_left;
			}else{
				gc_assign(t_node->m_value,t_value);
				return false;
			}
		}
	}
	t_node=(new c_Node8)->m_new(t_key,t_value,-1,t_parent);
	if((t_parent)!=0){
		if(t_cmp>0){
			gc_assign(t_parent->m_right,t_node);
		}else{
			gc_assign(t_parent->m_left,t_node);
		}
		p_InsertFixup3(t_node);
	}else{
		gc_assign(m_root,t_node);
	}
	return true;
}
bool c_Map3::p_Add3(String t_key,Object* t_value){
	c_Node8* t_node=m_root;
	c_Node8* t_parent=0;
	int t_cmp=0;
	while((t_node)!=0){
		t_parent=t_node;
		t_cmp=p_Compare6(t_key,t_node->m_key);
		if(t_cmp>0){
			t_node=t_node->m_right;
		}else{
			if(t_cmp<0){
				t_node=t_node->m_left;
			}else{
				return false;
			}
		}
	}
	t_node=(new c_Node8)->m_new(t_key,t_value,-1,t_parent);
	if((t_parent)!=0){
		if(t_cmp>0){
			gc_assign(t_parent->m_right,t_node);
		}else{
			gc_assign(t_parent->m_left,t_node);
		}
		p_InsertFixup3(t_node);
	}else{
		gc_assign(m_root,t_node);
	}
	return true;
}
bool c_Map3::p_Update3(String t_key,Object* t_value){
	c_Node8* t_node=p_FindNode3(t_key);
	if((t_node)!=0){
		gc_assign(t_node->m_value,t_value);
		return true;
	}
	return false;
}
Object* c_Map3::p_Get3(String t_key){
	c_Node8* t_node=p_FindNode3(t_key);
	if((t_node)!=0){
		return t_node->m_value;
	}
	return 0;
}
int c_Map3::p_DeleteFixup3(c_Node8* t_node,c_Node8* t_parent){
	while(t_node!=m_root && (!((t_node)!=0) || t_node->m_color==1)){
		if(t_node==t_parent->m_left){
			c_Node8* t_sib=t_parent->m_right;
			if(t_sib->m_color==-1){
				t_sib->m_color=1;
				t_parent->m_color=-1;
				p_RotateLeft3(t_parent);
				t_sib=t_parent->m_right;
			}
			if((!((t_sib->m_left)!=0) || t_sib->m_left->m_color==1) && (!((t_sib->m_right)!=0) || t_sib->m_right->m_color==1)){
				t_sib->m_color=-1;
				t_node=t_parent;
				t_parent=t_parent->m_parent;
			}else{
				if(!((t_sib->m_right)!=0) || t_sib->m_right->m_color==1){
					t_sib->m_left->m_color=1;
					t_sib->m_color=-1;
					p_RotateRight3(t_sib);
					t_sib=t_parent->m_right;
				}
				t_sib->m_color=t_parent->m_color;
				t_parent->m_color=1;
				t_sib->m_right->m_color=1;
				p_RotateLeft3(t_parent);
				t_node=m_root;
			}
		}else{
			c_Node8* t_sib2=t_parent->m_left;
			if(t_sib2->m_color==-1){
				t_sib2->m_color=1;
				t_parent->m_color=-1;
				p_RotateRight3(t_parent);
				t_sib2=t_parent->m_left;
			}
			if((!((t_sib2->m_right)!=0) || t_sib2->m_right->m_color==1) && (!((t_sib2->m_left)!=0) || t_sib2->m_left->m_color==1)){
				t_sib2->m_color=-1;
				t_node=t_parent;
				t_parent=t_parent->m_parent;
			}else{
				if(!((t_sib2->m_left)!=0) || t_sib2->m_left->m_color==1){
					t_sib2->m_right->m_color=1;
					t_sib2->m_color=-1;
					p_RotateLeft3(t_sib2);
					t_sib2=t_parent->m_left;
				}
				t_sib2->m_color=t_parent->m_color;
				t_parent->m_color=1;
				t_sib2->m_left->m_color=1;
				p_RotateRight3(t_parent);
				t_node=m_root;
			}
		}
	}
	if((t_node)!=0){
		t_node->m_color=1;
	}
	return 0;
}
int c_Map3::p_RemoveNode3(c_Node8* t_node){
	c_Node8* t_splice=0;
	c_Node8* t_child=0;
	if(!((t_node->m_left)!=0)){
		t_splice=t_node;
		t_child=t_node->m_right;
	}else{
		if(!((t_node->m_right)!=0)){
			t_splice=t_node;
			t_child=t_node->m_left;
		}else{
			t_splice=t_node->m_left;
			while((t_splice->m_right)!=0){
				t_splice=t_splice->m_right;
			}
			t_child=t_splice->m_left;
			t_node->m_key=t_splice->m_key;
			gc_assign(t_node->m_value,t_splice->m_value);
		}
	}
	c_Node8* t_parent=t_splice->m_parent;
	if((t_child)!=0){
		gc_assign(t_child->m_parent,t_parent);
	}
	if(!((t_parent)!=0)){
		gc_assign(m_root,t_child);
		return 0;
	}
	if(t_splice==t_parent->m_left){
		gc_assign(t_parent->m_left,t_child);
	}else{
		gc_assign(t_parent->m_right,t_child);
	}
	if(t_splice->m_color==1){
		p_DeleteFixup3(t_child,t_parent);
	}
	return 0;
}
int c_Map3::p_Remove4(String t_key){
	c_Node8* t_node=p_FindNode3(t_key);
	if(!((t_node)!=0)){
		return 0;
	}
	p_RemoveNode3(t_node);
	return 1;
}
c_MapKeys4* c_Map3::p_Keys(){
	return (new c_MapKeys4)->m_new(this);
}
c_MapValues3* c_Map3::p_Values(){
	return (new c_MapValues3)->m_new(this);
}
c_Node8* c_Map3::p_FirstNode(){
	if(!((m_root)!=0)){
		return 0;
	}
	c_Node8* t_node=m_root;
	while((t_node->m_left)!=0){
		t_node=t_node->m_left;
	}
	return t_node;
}
c_NodeEnumerator3* c_Map3::p_ObjectEnumerator(){
	return (new c_NodeEnumerator3)->m_new(p_FirstNode());
}
bool c_Map3::p_Insert6(String t_key,Object* t_value){
	return p_Set6(t_key,t_value);
}
Object* c_Map3::p_ValueForKey3(String t_key){
	return p_Get3(t_key);
}
c_Node8* c_Map3::p_LastNode(){
	if(!((m_root)!=0)){
		return 0;
	}
	c_Node8* t_node=m_root;
	while((t_node->m_right)!=0){
		t_node=t_node->m_right;
	}
	return t_node;
}
void c_Map3::mark(){
	Object::mark();
	gc_mark_q(m_root);
}
c_StringMap::c_StringMap(){
}
c_StringMap* c_StringMap::m_new(){
	c_Map3::m_new();
	return this;
}
int c_StringMap::p_Compare6(String t_lhs,String t_rhs){
	return t_lhs.Compare(t_rhs);
}
void c_StringMap::mark(){
	c_Map3::mark();
}
c_Stack::c_Stack(){
	m_data=Array<int >();
	m_length=0;
}
c_Stack* c_Stack::m_new(){
	return this;
}
c_Stack* c_Stack::m_new2(Array<int > t_data){
	gc_assign(this->m_data,t_data.Slice(0));
	this->m_length=t_data.Length();
	return this;
}
bool c_Stack::p_Equals5(int t_lhs,int t_rhs){
	return t_lhs==t_rhs;
}
int c_Stack::p_Compare4(int t_lhs,int t_rhs){
	bbError(String(L"Unable to compare items",23));
	return 0;
}
Array<int > c_Stack::p_ToArray(){
	Array<int > t_t=Array<int >(m_length);
	for(int t_i=0;t_i<m_length;t_i=t_i+1){
		t_t[t_i]=m_data[t_i];
	}
	return t_t;
}
int c_Stack::m_NIL;
void c_Stack::p_Clear(){
	for(int t_i=0;t_i<m_length;t_i=t_i+1){
		m_data[t_i]=m_NIL;
	}
	m_length=0;
}
void c_Stack::p_Length2(int t_newlength){
	if(t_newlength<m_length){
		for(int t_i=t_newlength;t_i<m_length;t_i=t_i+1){
			m_data[t_i]=m_NIL;
		}
	}else{
		if(t_newlength>m_data.Length()){
			gc_assign(m_data,m_data.Resize(bb_math_Max(m_length*2+10,t_newlength)));
		}
	}
	m_length=t_newlength;
}
int c_Stack::p_Length(){
	return m_length;
}
bool c_Stack::p_IsEmpty(){
	return m_length==0;
}
bool c_Stack::p_Contains(int t_value){
	for(int t_i=0;t_i<m_length;t_i=t_i+1){
		if(p_Equals5(m_data[t_i],t_value)){
			return true;
		}
	}
	return false;
}
void c_Stack::p_Push(int t_value){
	if(m_length==m_data.Length()){
		gc_assign(m_data,m_data.Resize(m_length*2+10));
	}
	m_data[m_length]=t_value;
	m_length+=1;
}
void c_Stack::p_Push2(Array<int > t_values,int t_offset,int t_count){
	for(int t_i=0;t_i<t_count;t_i=t_i+1){
		p_Push(t_values[t_offset+t_i]);
	}
}
void c_Stack::p_Push3(Array<int > t_values,int t_offset){
	p_Push2(t_values,t_offset,t_values.Length()-t_offset);
}
int c_Stack::p_Pop(){
	m_length-=1;
	int t_v=m_data[m_length];
	m_data[m_length]=m_NIL;
	return t_v;
}
int c_Stack::p_Top(){
	return m_data[m_length-1];
}
void c_Stack::p_Set(int t_index,int t_value){
	m_data[t_index]=t_value;
}
int c_Stack::p_Get(int t_index){
	return m_data[t_index];
}
int c_Stack::p_Find7(int t_value,int t_start){
	for(int t_i=t_start;t_i<m_length;t_i=t_i+1){
		if(p_Equals5(m_data[t_i],t_value)){
			return t_i;
		}
	}
	return -1;
}
int c_Stack::p_FindLast7(int t_value,int t_start){
	for(int t_i=t_start;t_i>=0;t_i=t_i+-1){
		if(p_Equals5(m_data[t_i],t_value)){
			return t_i;
		}
	}
	return -1;
}
int c_Stack::p_FindLast2(int t_value){
	return p_FindLast7(t_value,m_length-1);
}
void c_Stack::p_Insert7(int t_index,int t_value){
	if(m_length==m_data.Length()){
		gc_assign(m_data,m_data.Resize(m_length*2+10));
	}
	for(int t_i=m_length;t_i>t_index;t_i=t_i+-1){
		m_data[t_i]=m_data[t_i-1];
	}
	m_data[t_index]=t_value;
	m_length+=1;
}
void c_Stack::p_Remove(int t_index){
	for(int t_i=t_index;t_i<m_length-1;t_i=t_i+1){
		m_data[t_i]=m_data[t_i+1];
	}
	m_length-=1;
	m_data[m_length]=m_NIL;
}
void c_Stack::p_RemoveFirst2(int t_value){
	int t_i=p_Find7(t_value,0);
	if(t_i!=-1){
		p_Remove(t_i);
	}
}
void c_Stack::p_RemoveLast2(int t_value){
	int t_i=p_FindLast2(t_value);
	if(t_i!=-1){
		p_Remove(t_i);
	}
}
void c_Stack::p_RemoveEach(int t_value){
	int t_i=0;
	int t_j=m_length;
	while(t_i<m_length){
		if(!p_Equals5(m_data[t_i],t_value)){
			t_i+=1;
			continue;
		}
		int t_b=t_i;
		int t_e=t_i+1;
		while(t_e<m_length && p_Equals5(m_data[t_e],t_value)){
			t_e+=1;
		}
		while(t_e<m_length){
			m_data[t_b]=m_data[t_e];
			t_b+=1;
			t_e+=1;
		}
		m_length-=t_e-t_b;
		t_i+=1;
	}
	t_i=m_length;
	while(t_i<t_j){
		m_data[t_i]=m_NIL;
		t_i+=1;
	}
}
bool c_Stack::p__Less(int t_x,int t_y,int t_ascending){
	return p_Compare4(m_data[t_x],m_data[t_y])*t_ascending<0;
}
void c_Stack::p__Swap(int t_x,int t_y){
	int t_t=m_data[t_x];
	m_data[t_x]=m_data[t_y];
	m_data[t_y]=t_t;
}
bool c_Stack::p__Less2(int t_x,int t_y,int t_ascending){
	return p_Compare4(m_data[t_x],t_y)*t_ascending<0;
}
bool c_Stack::p__Less3(int t_x,int t_y,int t_ascending){
	return p_Compare4(t_x,m_data[t_y])*t_ascending<0;
}
void c_Stack::p__Sort(int t_lo,int t_hi,int t_ascending){
	if(t_hi<=t_lo){
		return;
	}
	if(t_lo+1==t_hi){
		if(p__Less(t_hi,t_lo,t_ascending)){
			p__Swap(t_hi,t_lo);
		}
		return;
	}
	int t_i=(t_hi-t_lo)/2+t_lo;
	if(p__Less(t_i,t_lo,t_ascending)){
		p__Swap(t_i,t_lo);
	}
	if(p__Less(t_hi,t_i,t_ascending)){
		p__Swap(t_hi,t_i);
		if(p__Less(t_i,t_lo,t_ascending)){
			p__Swap(t_i,t_lo);
		}
	}
	int t_x=t_lo+1;
	int t_y=t_hi-1;
	do{
		int t_p=m_data[t_i];
		while(p__Less2(t_x,t_p,t_ascending)){
			t_x+=1;
		}
		while(p__Less3(t_p,t_y,t_ascending)){
			t_y-=1;
		}
		if(t_x>t_y){
			break;
		}
		if(t_x<t_y){
			p__Swap(t_x,t_y);
			if(t_i==t_x){
				t_i=t_y;
			}else{
				if(t_i==t_y){
					t_i=t_x;
				}
			}
		}
		t_x+=1;
		t_y-=1;
	}while(!(t_x>t_y));
	p__Sort(t_lo,t_y,t_ascending);
	p__Sort(t_x,t_hi,t_ascending);
}
void c_Stack::p_Sort2(bool t_ascending){
	if(!((m_length)!=0)){
		return;
	}
	int t_t=1;
	if(!t_ascending){
		t_t=-1;
	}
	p__Sort(0,m_length-1,t_t);
}
c_Enumerator7* c_Stack::p_ObjectEnumerator(){
	return (new c_Enumerator7)->m_new(this);
}
c_BackwardsStack* c_Stack::p_Backwards(){
	return (new c_BackwardsStack)->m_new(this);
}
void c_Stack::mark(){
	Object::mark();
	gc_mark_q(m_data);
}
c_IntStack::c_IntStack(){
}
c_IntStack* c_IntStack::m_new(Array<int > t_data){
	c_Stack::m_new2(t_data);
	return this;
}
bool c_IntStack::p_Equals5(int t_lhs,int t_rhs){
	return t_lhs==t_rhs;
}
int c_IntStack::p_Compare4(int t_lhs,int t_rhs){
	return t_lhs-t_rhs;
}
c_IntStack* c_IntStack::m_new2(){
	c_Stack::m_new();
	return this;
}
void c_IntStack::mark(){
	c_Stack::mark();
}
c_Stack2::c_Stack2(){
	m_data=Array<Float >();
	m_length=0;
}
c_Stack2* c_Stack2::m_new(){
	return this;
}
c_Stack2* c_Stack2::m_new2(Array<Float > t_data){
	gc_assign(this->m_data,t_data.Slice(0));
	this->m_length=t_data.Length();
	return this;
}
bool c_Stack2::p_Equals6(Float t_lhs,Float t_rhs){
	return t_lhs==t_rhs;
}
int c_Stack2::p_Compare5(Float t_lhs,Float t_rhs){
	bbError(String(L"Unable to compare items",23));
	return 0;
}
Array<Float > c_Stack2::p_ToArray(){
	Array<Float > t_t=Array<Float >(m_length);
	for(int t_i=0;t_i<m_length;t_i=t_i+1){
		t_t[t_i]=m_data[t_i];
	}
	return t_t;
}
Float c_Stack2::m_NIL;
void c_Stack2::p_Clear(){
	for(int t_i=0;t_i<m_length;t_i=t_i+1){
		m_data[t_i]=m_NIL;
	}
	m_length=0;
}
void c_Stack2::p_Length2(int t_newlength){
	if(t_newlength<m_length){
		for(int t_i=t_newlength;t_i<m_length;t_i=t_i+1){
			m_data[t_i]=m_NIL;
		}
	}else{
		if(t_newlength>m_data.Length()){
			gc_assign(m_data,m_data.Resize(bb_math_Max(m_length*2+10,t_newlength)));
		}
	}
	m_length=t_newlength;
}
int c_Stack2::p_Length(){
	return m_length;
}
bool c_Stack2::p_IsEmpty(){
	return m_length==0;
}
bool c_Stack2::p_Contains2(Float t_value){
	for(int t_i=0;t_i<m_length;t_i=t_i+1){
		if(p_Equals6(m_data[t_i],t_value)){
			return true;
		}
	}
	return false;
}
void c_Stack2::p_Push4(Float t_value){
	if(m_length==m_data.Length()){
		gc_assign(m_data,m_data.Resize(m_length*2+10));
	}
	m_data[m_length]=t_value;
	m_length+=1;
}
void c_Stack2::p_Push5(Array<Float > t_values,int t_offset,int t_count){
	for(int t_i=0;t_i<t_count;t_i=t_i+1){
		p_Push4(t_values[t_offset+t_i]);
	}
}
void c_Stack2::p_Push6(Array<Float > t_values,int t_offset){
	p_Push5(t_values,t_offset,t_values.Length()-t_offset);
}
Float c_Stack2::p_Pop(){
	m_length-=1;
	Float t_v=m_data[m_length];
	m_data[m_length]=m_NIL;
	return t_v;
}
Float c_Stack2::p_Top(){
	return m_data[m_length-1];
}
void c_Stack2::p_Set2(int t_index,Float t_value){
	m_data[t_index]=t_value;
}
Float c_Stack2::p_Get(int t_index){
	return m_data[t_index];
}
int c_Stack2::p_Find8(Float t_value,int t_start){
	for(int t_i=t_start;t_i<m_length;t_i=t_i+1){
		if(p_Equals6(m_data[t_i],t_value)){
			return t_i;
		}
	}
	return -1;
}
int c_Stack2::p_FindLast8(Float t_value,int t_start){
	for(int t_i=t_start;t_i>=0;t_i=t_i+-1){
		if(p_Equals6(m_data[t_i],t_value)){
			return t_i;
		}
	}
	return -1;
}
int c_Stack2::p_FindLast4(Float t_value){
	return p_FindLast8(t_value,m_length-1);
}
void c_Stack2::p_Insert8(int t_index,Float t_value){
	if(m_length==m_data.Length()){
		gc_assign(m_data,m_data.Resize(m_length*2+10));
	}
	for(int t_i=m_length;t_i>t_index;t_i=t_i+-1){
		m_data[t_i]=m_data[t_i-1];
	}
	m_data[t_index]=t_value;
	m_length+=1;
}
void c_Stack2::p_Remove(int t_index){
	for(int t_i=t_index;t_i<m_length-1;t_i=t_i+1){
		m_data[t_i]=m_data[t_i+1];
	}
	m_length-=1;
	m_data[m_length]=m_NIL;
}
void c_Stack2::p_RemoveFirst3(Float t_value){
	int t_i=p_Find8(t_value,0);
	if(t_i!=-1){
		p_Remove(t_i);
	}
}
void c_Stack2::p_RemoveLast3(Float t_value){
	int t_i=p_FindLast4(t_value);
	if(t_i!=-1){
		p_Remove(t_i);
	}
}
void c_Stack2::p_RemoveEach2(Float t_value){
	int t_i=0;
	int t_j=m_length;
	while(t_i<m_length){
		if(!p_Equals6(m_data[t_i],t_value)){
			t_i+=1;
			continue;
		}
		int t_b=t_i;
		int t_e=t_i+1;
		while(t_e<m_length && p_Equals6(m_data[t_e],t_value)){
			t_e+=1;
		}
		while(t_e<m_length){
			m_data[t_b]=m_data[t_e];
			t_b+=1;
			t_e+=1;
		}
		m_length-=t_e-t_b;
		t_i+=1;
	}
	t_i=m_length;
	while(t_i<t_j){
		m_data[t_i]=m_NIL;
		t_i+=1;
	}
}
bool c_Stack2::p__Less(int t_x,int t_y,int t_ascending){
	return p_Compare5(m_data[t_x],m_data[t_y])*t_ascending<0;
}
void c_Stack2::p__Swap(int t_x,int t_y){
	Float t_t=m_data[t_x];
	m_data[t_x]=m_data[t_y];
	m_data[t_y]=t_t;
}
bool c_Stack2::p__Less22(int t_x,Float t_y,int t_ascending){
	return p_Compare5(m_data[t_x],t_y)*t_ascending<0;
}
bool c_Stack2::p__Less32(Float t_x,int t_y,int t_ascending){
	return p_Compare5(t_x,m_data[t_y])*t_ascending<0;
}
void c_Stack2::p__Sort(int t_lo,int t_hi,int t_ascending){
	if(t_hi<=t_lo){
		return;
	}
	if(t_lo+1==t_hi){
		if(p__Less(t_hi,t_lo,t_ascending)){
			p__Swap(t_hi,t_lo);
		}
		return;
	}
	int t_i=(t_hi-t_lo)/2+t_lo;
	if(p__Less(t_i,t_lo,t_ascending)){
		p__Swap(t_i,t_lo);
	}
	if(p__Less(t_hi,t_i,t_ascending)){
		p__Swap(t_hi,t_i);
		if(p__Less(t_i,t_lo,t_ascending)){
			p__Swap(t_i,t_lo);
		}
	}
	int t_x=t_lo+1;
	int t_y=t_hi-1;
	do{
		Float t_p=m_data[t_i];
		while(p__Less22(t_x,t_p,t_ascending)){
			t_x+=1;
		}
		while(p__Less32(t_p,t_y,t_ascending)){
			t_y-=1;
		}
		if(t_x>t_y){
			break;
		}
		if(t_x<t_y){
			p__Swap(t_x,t_y);
			if(t_i==t_x){
				t_i=t_y;
			}else{
				if(t_i==t_y){
					t_i=t_x;
				}
			}
		}
		t_x+=1;
		t_y-=1;
	}while(!(t_x>t_y));
	p__Sort(t_lo,t_y,t_ascending);
	p__Sort(t_x,t_hi,t_ascending);
}
void c_Stack2::p_Sort2(bool t_ascending){
	if(!((m_length)!=0)){
		return;
	}
	int t_t=1;
	if(!t_ascending){
		t_t=-1;
	}
	p__Sort(0,m_length-1,t_t);
}
c_Enumerator8* c_Stack2::p_ObjectEnumerator(){
	return (new c_Enumerator8)->m_new(this);
}
c_BackwardsStack2* c_Stack2::p_Backwards(){
	return (new c_BackwardsStack2)->m_new(this);
}
void c_Stack2::mark(){
	Object::mark();
	gc_mark_q(m_data);
}
c_FloatStack::c_FloatStack(){
}
c_FloatStack* c_FloatStack::m_new(Array<Float > t_data){
	c_Stack2::m_new2(t_data);
	return this;
}
bool c_FloatStack::p_Equals6(Float t_lhs,Float t_rhs){
	return t_lhs==t_rhs;
}
int c_FloatStack::p_Compare5(Float t_lhs,Float t_rhs){
	if(t_lhs<t_rhs){
		return -1;
	}
	return ((t_lhs>t_rhs)?1:0);
}
c_FloatStack* c_FloatStack::m_new2(){
	c_Stack2::m_new();
	return this;
}
void c_FloatStack::mark(){
	c_Stack2::mark();
}
c_Stack3::c_Stack3(){
	m_data=Array<String >();
	m_length=0;
}
c_Stack3* c_Stack3::m_new(){
	return this;
}
c_Stack3* c_Stack3::m_new2(Array<String > t_data){
	gc_assign(this->m_data,t_data.Slice(0));
	this->m_length=t_data.Length();
	return this;
}
Array<String > c_Stack3::p_ToArray(){
	Array<String > t_t=Array<String >(m_length);
	for(int t_i=0;t_i<m_length;t_i=t_i+1){
		t_t[t_i]=m_data[t_i];
	}
	return t_t;
}
bool c_Stack3::p_Equals7(String t_lhs,String t_rhs){
	return t_lhs==t_rhs;
}
int c_Stack3::p_Compare6(String t_lhs,String t_rhs){
	bbError(String(L"Unable to compare items",23));
	return 0;
}
String c_Stack3::m_NIL;
void c_Stack3::p_Clear(){
	for(int t_i=0;t_i<m_length;t_i=t_i+1){
		m_data[t_i]=m_NIL;
	}
	m_length=0;
}
void c_Stack3::p_Length2(int t_newlength){
	if(t_newlength<m_length){
		for(int t_i=t_newlength;t_i<m_length;t_i=t_i+1){
			m_data[t_i]=m_NIL;
		}
	}else{
		if(t_newlength>m_data.Length()){
			gc_assign(m_data,m_data.Resize(bb_math_Max(m_length*2+10,t_newlength)));
		}
	}
	m_length=t_newlength;
}
int c_Stack3::p_Length(){
	return m_length;
}
bool c_Stack3::p_IsEmpty(){
	return m_length==0;
}
bool c_Stack3::p_Contains3(String t_value){
	for(int t_i=0;t_i<m_length;t_i=t_i+1){
		if(p_Equals7(m_data[t_i],t_value)){
			return true;
		}
	}
	return false;
}
void c_Stack3::p_Push7(String t_value){
	if(m_length==m_data.Length()){
		gc_assign(m_data,m_data.Resize(m_length*2+10));
	}
	m_data[m_length]=t_value;
	m_length+=1;
}
void c_Stack3::p_Push8(Array<String > t_values,int t_offset,int t_count){
	for(int t_i=0;t_i<t_count;t_i=t_i+1){
		p_Push7(t_values[t_offset+t_i]);
	}
}
void c_Stack3::p_Push9(Array<String > t_values,int t_offset){
	p_Push8(t_values,t_offset,t_values.Length()-t_offset);
}
String c_Stack3::p_Pop(){
	m_length-=1;
	String t_v=m_data[m_length];
	m_data[m_length]=m_NIL;
	return t_v;
}
String c_Stack3::p_Top(){
	return m_data[m_length-1];
}
void c_Stack3::p_Set3(int t_index,String t_value){
	m_data[t_index]=t_value;
}
String c_Stack3::p_Get(int t_index){
	return m_data[t_index];
}
int c_Stack3::p_Find9(String t_value,int t_start){
	for(int t_i=t_start;t_i<m_length;t_i=t_i+1){
		if(p_Equals7(m_data[t_i],t_value)){
			return t_i;
		}
	}
	return -1;
}
int c_Stack3::p_FindLast9(String t_value,int t_start){
	for(int t_i=t_start;t_i>=0;t_i=t_i+-1){
		if(p_Equals7(m_data[t_i],t_value)){
			return t_i;
		}
	}
	return -1;
}
int c_Stack3::p_FindLast6(String t_value){
	return p_FindLast9(t_value,m_length-1);
}
void c_Stack3::p_Insert9(int t_index,String t_value){
	if(m_length==m_data.Length()){
		gc_assign(m_data,m_data.Resize(m_length*2+10));
	}
	for(int t_i=m_length;t_i>t_index;t_i=t_i+-1){
		m_data[t_i]=m_data[t_i-1];
	}
	m_data[t_index]=t_value;
	m_length+=1;
}
void c_Stack3::p_Remove(int t_index){
	for(int t_i=t_index;t_i<m_length-1;t_i=t_i+1){
		m_data[t_i]=m_data[t_i+1];
	}
	m_length-=1;
	m_data[m_length]=m_NIL;
}
void c_Stack3::p_RemoveFirst4(String t_value){
	int t_i=p_Find9(t_value,0);
	if(t_i!=-1){
		p_Remove(t_i);
	}
}
void c_Stack3::p_RemoveLast4(String t_value){
	int t_i=p_FindLast6(t_value);
	if(t_i!=-1){
		p_Remove(t_i);
	}
}
void c_Stack3::p_RemoveEach3(String t_value){
	int t_i=0;
	int t_j=m_length;
	while(t_i<m_length){
		if(!p_Equals7(m_data[t_i],t_value)){
			t_i+=1;
			continue;
		}
		int t_b=t_i;
		int t_e=t_i+1;
		while(t_e<m_length && p_Equals7(m_data[t_e],t_value)){
			t_e+=1;
		}
		while(t_e<m_length){
			m_data[t_b]=m_data[t_e];
			t_b+=1;
			t_e+=1;
		}
		m_length-=t_e-t_b;
		t_i+=1;
	}
	t_i=m_length;
	while(t_i<t_j){
		m_data[t_i]=m_NIL;
		t_i+=1;
	}
}
bool c_Stack3::p__Less(int t_x,int t_y,int t_ascending){
	return p_Compare6(m_data[t_x],m_data[t_y])*t_ascending<0;
}
void c_Stack3::p__Swap(int t_x,int t_y){
	String t_t=m_data[t_x];
	m_data[t_x]=m_data[t_y];
	m_data[t_y]=t_t;
}
bool c_Stack3::p__Less23(int t_x,String t_y,int t_ascending){
	return p_Compare6(m_data[t_x],t_y)*t_ascending<0;
}
bool c_Stack3::p__Less33(String t_x,int t_y,int t_ascending){
	return p_Compare6(t_x,m_data[t_y])*t_ascending<0;
}
void c_Stack3::p__Sort(int t_lo,int t_hi,int t_ascending){
	if(t_hi<=t_lo){
		return;
	}
	if(t_lo+1==t_hi){
		if(p__Less(t_hi,t_lo,t_ascending)){
			p__Swap(t_hi,t_lo);
		}
		return;
	}
	int t_i=(t_hi-t_lo)/2+t_lo;
	if(p__Less(t_i,t_lo,t_ascending)){
		p__Swap(t_i,t_lo);
	}
	if(p__Less(t_hi,t_i,t_ascending)){
		p__Swap(t_hi,t_i);
		if(p__Less(t_i,t_lo,t_ascending)){
			p__Swap(t_i,t_lo);
		}
	}
	int t_x=t_lo+1;
	int t_y=t_hi-1;
	do{
		String t_p=m_data[t_i];
		while(p__Less23(t_x,t_p,t_ascending)){
			t_x+=1;
		}
		while(p__Less33(t_p,t_y,t_ascending)){
			t_y-=1;
		}
		if(t_x>t_y){
			break;
		}
		if(t_x<t_y){
			p__Swap(t_x,t_y);
			if(t_i==t_x){
				t_i=t_y;
			}else{
				if(t_i==t_y){
					t_i=t_x;
				}
			}
		}
		t_x+=1;
		t_y-=1;
	}while(!(t_x>t_y));
	p__Sort(t_lo,t_y,t_ascending);
	p__Sort(t_x,t_hi,t_ascending);
}
void c_Stack3::p_Sort2(bool t_ascending){
	if(!((m_length)!=0)){
		return;
	}
	int t_t=1;
	if(!t_ascending){
		t_t=-1;
	}
	p__Sort(0,m_length-1,t_t);
}
c_Enumerator9* c_Stack3::p_ObjectEnumerator(){
	return (new c_Enumerator9)->m_new(this);
}
c_BackwardsStack3* c_Stack3::p_Backwards(){
	return (new c_BackwardsStack3)->m_new(this);
}
void c_Stack3::mark(){
	Object::mark();
	gc_mark_q(m_data);
}
c_StringStack::c_StringStack(){
}
c_StringStack* c_StringStack::m_new(Array<String > t_data){
	c_Stack3::m_new2(t_data);
	return this;
}
String c_StringStack::p_Join(String t_separator){
	return t_separator.Join(p_ToArray());
}
bool c_StringStack::p_Equals7(String t_lhs,String t_rhs){
	return t_lhs==t_rhs;
}
int c_StringStack::p_Compare6(String t_lhs,String t_rhs){
	return t_lhs.Compare(t_rhs);
}
c_StringStack* c_StringStack::m_new2(){
	c_Stack3::m_new();
	return this;
}
void c_StringStack::mark(){
	c_Stack3::mark();
}
c_Color::c_Color(){
	m_red=FLOAT(1.0);
	m_green=FLOAT(1.0);
	m_blue=FLOAT(1.0);
	m_alpha=FLOAT(1.0);
}
void c_Color::p_RGB(int t_rgb){
	m_red=Float(t_rgb>>16&255)/FLOAT(255.0);
	m_green=Float(t_rgb>>8&255)/FLOAT(255.0);
	m_blue=Float(t_rgb&255)/FLOAT(255.0);
}
int c_Color::p_RGB2(){
	return int(m_red*FLOAT(255.0))<<16|int(m_green*FLOAT(255.0))<<8|int(m_blue*FLOAT(255.0));
}
c_Color* c_Color::m_new(int t_rgb){
	p_RGB(t_rgb);
	return this;
}
Float c_Color::p_Red(){
	return m_red;
}
void c_Color::p_Red2(Float t_value){
	m_red=bb_math_Clamp2(t_value,FLOAT(0.0),FLOAT(1.0));
}
Float c_Color::p_Green(){
	return m_green;
}
void c_Color::p_Green2(Float t_value){
	m_green=bb_math_Clamp2(t_value,FLOAT(0.0),FLOAT(1.0));
}
Float c_Color::p_Blue(){
	return m_blue;
}
void c_Color::p_Blue2(Float t_value){
	m_blue=bb_math_Clamp2(t_value,FLOAT(0.0),FLOAT(1.0));
}
Float c_Color::p_Alpha(){
	return m_alpha;
}
void c_Color::p_Alpha2(Float t_value){
	m_alpha=bb_math_Clamp2(t_value,FLOAT(0.0),FLOAT(1.0));
}
void c_Color::p_Set7(Float t_red,Float t_green,Float t_blue,Float t_alpha){
	p_Red2(t_red);
	p_Green2(t_green);
	p_Blue2(t_blue);
	p_Alpha2(t_alpha);
}
void c_Color::p_Set8(c_Color* t_newColor){
	m_red=t_newColor->m_red;
	m_green=t_newColor->m_green;
	m_blue=t_newColor->m_blue;
	m_alpha=t_newColor->m_alpha;
}
void c_Color::p_Set9(int t_rgb){
	p_RGB(t_rgb);
}
c_Color* c_Color::m_new2(Float t_red,Float t_green,Float t_blue,Float t_alpha){
	p_Set7(t_red,t_green,t_blue,t_alpha);
	return this;
}
c_Color* c_Color::m_new3(c_Color* t_withColor){
	this->p_Set8(t_withColor);
	return this;
}
c_Color* c_Color::m_new4(){
	return this;
}
c_ImmutableColor* c_Color::m_Black;
c_ImmutableColor* c_Color::m_White;
c_ImmutableColor* c_Color::m_PureRed;
c_ImmutableColor* c_Color::m_PureGreen;
c_ImmutableColor* c_Color::m_PureBlue;
c_ImmutableColor* c_Color::m_Navy;
c_ImmutableColor* c_Color::m_NewBlue;
c_ImmutableColor* c_Color::m_Aqua;
c_ImmutableColor* c_Color::m_Teal;
c_ImmutableColor* c_Color::m_Olive;
c_ImmutableColor* c_Color::m_NewGreen;
c_ImmutableColor* c_Color::m_Lime;
c_ImmutableColor* c_Color::m_Yellow;
c_ImmutableColor* c_Color::m_Orange;
c_ImmutableColor* c_Color::m_NewRed;
c_ImmutableColor* c_Color::m_Maroon;
c_ImmutableColor* c_Color::m_Fuchsia;
c_ImmutableColor* c_Color::m_Purple;
c_ImmutableColor* c_Color::m_Silver;
c_ImmutableColor* c_Color::m_Gray;
c_ImmutableColor* c_Color::m_NewBlack;
void c_Color::p_Reset(){
	m_red=FLOAT(1.0);
	m_green=FLOAT(1.0);
	m_blue=FLOAT(1.0);
	m_alpha=FLOAT(1.0);
}
int c_Color::p_ARGB(){
	return int(m_alpha*FLOAT(255.0))<<24|int(m_red*FLOAT(255.0))<<16|int(m_green*FLOAT(255.0))<<8|int(m_blue*FLOAT(255.0));
}
void c_Color::p_Randomize(){
	m_red=bb_random_Rnd();
	m_green=bb_random_Rnd();
	m_blue=bb_random_Rnd();
}
bool c_Color::p_Equals8(c_Color* t_color){
	if(t_color->m_red!=m_red){
		return false;
	}
	if(t_color->m_green!=m_green){
		return false;
	}
	if(t_color->m_blue!=m_blue){
		return false;
	}
	if(t_color->m_alpha!=m_alpha){
		return false;
	}
	return true;
}
String c_Color::p_ToString(){
	return String(L"(Red: ",6)+String(m_red)+String(L" Green: ",8)+String(m_green)+String(L" Blue: ",7)+String(m_blue)+String(L" Alpha: ",8)+String(m_alpha)+String(L")",1);
}
void c_Color::p_Use(){
	bb_graphics_SetColor(m_red*FLOAT(255.0),m_green*FLOAT(255.0),m_blue*FLOAT(255.0));
	bb_graphics_SetAlpha(m_alpha);
}
void c_Color::p_UseWithoutAlpha(){
	bb_graphics_SetColor(m_red*FLOAT(255.0),m_green*FLOAT(255.0),m_blue*FLOAT(255.0));
}
void c_Color::mark(){
	Object::mark();
}
c_ImmutableColor::c_ImmutableColor(){
}
c_ImmutableColor* c_ImmutableColor::m_new(){
	c_Color::m_new4();
	bb_functions2_NoDefaultConstructorError(String(L"ImmutableColor",14));
	return this;
}
c_ImmutableColor* c_ImmutableColor::m_new2(int t_rgb){
	c_Color::m_new4();
	c_Color::p_RGB(t_rgb);
	return this;
}
c_ImmutableColor* c_ImmutableColor::m_new3(Float t_red,Float t_green,Float t_blue,Float t_alpha){
	c_Color::m_new4();
	c_Color::p_Red2(t_red);
	c_Color::p_Green2(t_green);
	c_Color::p_Blue2(t_blue);
	c_Color::p_Alpha2(t_alpha);
	return this;
}
void c_ImmutableColor::p_CantChangeError(){
	bbError(String(L"ImmutableColor can't be changed.\n",33)+this->p_ToString());
}
void c_ImmutableColor::p_Set7(Float t_red,Float t_green,Float t_blue,Float t_alpha){
	p_CantChangeError();
}
void c_ImmutableColor::p_Set8(c_Color* t_newColor){
	p_CantChangeError();
}
void c_ImmutableColor::p_Reset(){
	p_CantChangeError();
}
void c_ImmutableColor::p_Randomize(){
	p_CantChangeError();
}
void c_ImmutableColor::p_Red2(Float t_value){
	p_CantChangeError();
}
void c_ImmutableColor::p_Green2(Float t_value){
	p_CantChangeError();
}
void c_ImmutableColor::p_Blue2(Float t_value){
	p_CantChangeError();
}
void c_ImmutableColor::p_Alpha2(Float t_value){
	p_CantChangeError();
}
void c_ImmutableColor::p_RGB(int t_rgb){
	p_CantChangeError();
}
void c_ImmutableColor::mark(){
	c_Color::mark();
}
void bb_functions2_NoDefaultConstructorError(String t_className){
	bbError(t_className+String(L": Use of default constructor is not allowed.",44));
}
c_GraphicsContext::c_GraphicsContext(){
	m_color_r=FLOAT(.0);
	m_color_g=FLOAT(.0);
	m_color_b=FLOAT(.0);
	m_alpha=FLOAT(.0);
	m_ix=FLOAT(1.0);
	m_jx=FLOAT(.0);
	m_iy=FLOAT(.0);
	m_jy=FLOAT(1.0);
	m_tx=FLOAT(.0);
	m_ty=FLOAT(.0);
	m_tformed=0;
	m_matDirty=0;
	m_matrixSp=0;
	m_matrixStack=Array<Float >(192);
	m_defaultFont=0;
	m_font=0;
	m_firstChar=0;
	m_blend=0;
	m_scissor_x=FLOAT(.0);
	m_scissor_y=FLOAT(.0);
	m_scissor_width=FLOAT(.0);
	m_scissor_height=FLOAT(.0);
}
c_GraphicsContext* c_GraphicsContext::m_new(){
	return this;
}
int c_GraphicsContext::p_Validate(){
	if((m_matDirty)!=0){
		bb_graphics_renderDevice->SetMatrix(bb_graphics_context->m_ix,bb_graphics_context->m_iy,bb_graphics_context->m_jx,bb_graphics_context->m_jy,bb_graphics_context->m_tx,bb_graphics_context->m_ty);
		m_matDirty=0;
	}
	return 0;
}
void c_GraphicsContext::mark(){
	Object::mark();
	gc_mark_q(m_matrixStack);
	gc_mark_q(m_defaultFont);
	gc_mark_q(m_font);
}
c_GraphicsContext* bb_graphics_context;
gxtkGraphics* bb_graphics_renderDevice;
int bb_graphics_SetColor(Float t_r,Float t_g,Float t_b){
	bb_graphics_context->m_color_r=t_r;
	bb_graphics_context->m_color_g=t_g;
	bb_graphics_context->m_color_b=t_b;
	bb_graphics_renderDevice->SetColor(t_r,t_g,t_b);
	return 0;
}
int bb_graphics_SetAlpha(Float t_alpha){
	bb_graphics_context->m_alpha=t_alpha;
	bb_graphics_renderDevice->SetAlpha(t_alpha);
	return 0;
}
c_Vec2::c_Vec2(){
	m_x=FLOAT(.0);
	m_y=FLOAT(.0);
}
c_Vec2* c_Vec2::m_new(Float t_setX,Float t_setY){
	m_x=t_setX;
	m_y=t_setY;
	return this;
}
c_Vec2* c_Vec2::m_new2(c_Vec2* t_vector){
	m_x=t_vector->m_x;
	m_y=t_vector->m_y;
	return this;
}
c_Vec2* c_Vec2::m_new3(){
	return this;
}
c_Vec2* c_Vec2::p_Copy(){
	return (new c_Vec2)->m_new2(this);
}
c_Vec2* c_Vec2::m_FromPolar(Float t_radius,Float t_theta){
	c_Vec2* t_v=(new c_Vec2)->m_new3();
	t_v->m_x=t_radius*(Float)cos((t_theta)*D2R);
	t_v->m_y=t_radius*(Float)sin((t_theta)*D2R);
	return t_v;
}
c_Vec2* c_Vec2::p_Set10(Float t_setX,Float t_setY){
	m_x=t_setX;
	m_y=t_setY;
	return this;
}
c_Vec2* c_Vec2::p_Set11(c_Vec2* t_vector){
	m_x=t_vector->m_x;
	m_y=t_vector->m_y;
	return this;
}
c_Vec2* c_Vec2::p_Add4(c_Vec2* t_vector){
	m_x+=t_vector->m_x;
	m_y+=t_vector->m_y;
	return this;
}
c_Vec2* c_Vec2::p_Add5(Float t_addX,Float t_addY){
	m_x+=t_addX;
	m_y+=t_addY;
	return this;
}
c_Vec2* c_Vec2::p_Sub(c_Vec2* t_vector){
	m_x-=t_vector->m_x;
	m_y-=t_vector->m_y;
	return this;
}
c_Vec2* c_Vec2::p_Sub2(Float t_subX,Float t_subY){
	m_x-=t_subX;
	m_y-=t_subY;
	return this;
}
c_Vec2* c_Vec2::p_Mul(Float t_scalar){
	m_x*=t_scalar;
	m_y*=t_scalar;
	return this;
}
c_Vec2* c_Vec2::p_Div(Float t_scalar){
	m_x/=t_scalar;
	m_y/=t_scalar;
	return this;
}
Float c_Vec2::p_Length(){
	return (Float)sqrt(m_x*m_x+m_y*m_y);
}
c_Vec2* c_Vec2::m_Mul(c_Vec2* t_a,Float t_scalar){
	return ((new c_Vec2)->m_new2(t_a))->p_Mul(t_scalar);
}
void c_Vec2::p_Length3(Float t_length){
	p_Normalize();
	this->p_Mul(t_length);
}
c_Vec2* c_Vec2::p_Normalize(){
	Float t_length=this->p_Length();
	if(p_Length()==FLOAT(0.0)){
		return this;
	}
	p_Set10(m_x/t_length,m_y/t_length);
	return this;
}
Float c_Vec2::p_Dot(c_Vec2* t_vector){
	return m_x*t_vector->m_x+m_y*t_vector->m_y;
}
void c_Vec2::p_Limit(Float t_maxLength){
	Float t_length=this->p_Length();
	if(t_length>t_maxLength){
		this->p_Length3(t_maxLength);
	}
}
Float c_Vec2::p_LengthSquared(){
	return m_x*m_x+m_y*m_y;
}
Float c_Vec2::p_Angle(){
	return (Float)(atan2(this->m_y,this->m_x)*R2D);
}
void c_Vec2::p_Angle2(Float t_value){
	Float t_length=p_Length();
	this->m_x=(Float)cos((t_value)*D2R)*t_length;
	this->m_y=(Float)sin((t_value)*D2R)*t_length;
}
void c_Vec2::p_RotateLeft4(){
	Float t_temp=-m_y;
	m_y=m_x;
	m_x=t_temp;
}
Float c_Vec2::p_DistanceTo(Float t_x,Float t_y){
	Float t_x1=t_x-this->m_x;
	Float t_y1=t_y-this->m_y;
	return (Float)sqrt(t_x1*t_x1+t_y1*t_y1);
}
Float c_Vec2::p_DistanceTo2(c_Vec2* t_vector){
	return p_DistanceTo(t_vector->m_x,t_vector->m_y);
}
bool c_Vec2::p_Equals9(c_Vec2* t_vector){
	return m_x==t_vector->m_x && m_y==t_vector->m_y;
}
Float c_Vec2::m_Dot(c_Vec2* t_a,c_Vec2* t_b){
	return t_a->p_Dot(t_b);
}
Float c_Vec2::p_ProjectOn(c_Vec2* t_axis){
	c_Vec2* t_normalizedAxis=(new c_Vec2)->m_new2(t_axis);
	t_normalizedAxis->p_Normalize();
	return this->p_Dot(t_normalizedAxis);
}
String c_Vec2::p_ToString(){
	return String(L"x: ",3)+String(m_x)+String(L", y: ",5)+String(m_y);
}
c_Vec2* c_Vec2::m_Up(){
	return (new c_Vec2)->m_new(FLOAT(0.0),FLOAT(-1.0));
}
c_Vec2* c_Vec2::m_Down(){
	return (new c_Vec2)->m_new(FLOAT(0.0),FLOAT(1.0));
}
c_Vec2* c_Vec2::m_Left(){
	return (new c_Vec2)->m_new(FLOAT(-1.0),FLOAT(0.0));
}
c_Vec2* c_Vec2::m_Right(){
	return (new c_Vec2)->m_new(FLOAT(1.0),FLOAT(0.0));
}
c_Vec2* c_Vec2::m_Add(c_Vec2* t_a,c_Vec2* t_b){
	return ((new c_Vec2)->m_new2(t_a))->p_Add4(t_b);
}
c_Vec2* c_Vec2::m_Sub(c_Vec2* t_a,c_Vec2* t_b){
	return ((new c_Vec2)->m_new2(t_a))->p_Sub(t_b);
}
c_Vec2* c_Vec2::m_Div(c_Vec2* t_a,Float t_scalar){
	return ((new c_Vec2)->m_new2(t_a))->p_Div(t_scalar);
}
Float c_Vec2::m_AngleBetween(c_Vec2* t_a,c_Vec2* t_b){
	Float t_dotProduct=t_a->p_Dot(t_b);
	Float t_result=t_dotProduct/(t_a->p_Length()*t_b->p_Length());
	return (Float)(acos(t_result)*R2D);
}
void c_Vec2::mark(){
	Object::mark();
}
c_VEntity::c_VEntity(){
	m_attributes=0;
	m_position=(new c_Vec2)->m_new3();
	m_scale=(new c_Vec2)->m_new(FLOAT(1.0),FLOAT(1.0));
	m_rotation=FLOAT(.0);
}
void c_VEntity::p_SetAttribute(String t_name,String t_value){
	if(!((m_attributes)!=0)){
		gc_assign(m_attributes,(new c_StringMap2)->m_new());
	}
	m_attributes->p_Set12(t_name,t_value);
}
void c_VEntity::p_SetAttribute2(String t_name,bool t_value){
	if(!((m_attributes)!=0)){
		gc_assign(m_attributes,(new c_StringMap2)->m_new());
	}
	m_attributes->p_Set12(t_name,String((t_value)?1:0));
}
String c_VEntity::p_GetAttribute(String t_name){
	if(!((m_attributes)!=0)){
		return String();
	}
	return m_attributes->p_Get3(t_name);
}
bool c_VEntity::p_HasAttribute(String t_name){
	if(!((m_attributes)!=0)){
		return false;
	}
	return m_attributes->p_Contains3(t_name);
}
int c_VEntity::p_NumberOfAttributes(){
	if(!((m_attributes)!=0)){
		return 0;
	}
	return m_attributes->p_Count();
}
c_StringMap2* c_VEntity::p_GetAttributeMap(){
	if(!((m_attributes)!=0)){
		return 0;
	}
	c_StringMap2* t_map=(new c_StringMap2)->m_new();
	c_KeyEnumerator* t_=m_attributes->p_Keys()->p_ObjectEnumerator();
	while(t_->p_HasNext()){
		String t_key=t_->p_NextObject();
		String t_value=m_attributes->p_Get3(t_key);
		t_map->p_Set12(t_key,t_value);
	}
	return t_map;
}
void c_VEntity::p_SetAttributeMap(c_StringMap2* t_map){
	gc_assign(m_attributes,t_map);
}
void c_VEntity::p_SetScale(Float t_scalar){
	this->m_scale->p_Set10(t_scalar,t_scalar);
}
void c_VEntity::p_ApplyTransform(){
	bb_functions_TranslateV(m_position);
	bb_graphics_Rotate(m_rotation);
	bb_functions_ScaleV(m_scale);
}
void c_VEntity::p_Update4(Float t_dt){
}
void c_VEntity::p_Render(){
}
c_VEntity* c_VEntity::m_new(){
	return this;
}
void c_VEntity::mark(){
	Object::mark();
	gc_mark_q(m_attributes);
	gc_mark_q(m_position);
	gc_mark_q(m_scale);
}
c_Map4::c_Map4(){
	m_root=0;
}
c_Map4* c_Map4::m_new(){
	return this;
}
int c_Map4::p_RotateLeft5(c_Node4* t_node){
	c_Node4* t_child=t_node->m_right;
	gc_assign(t_node->m_right,t_child->m_left);
	if((t_child->m_left)!=0){
		gc_assign(t_child->m_left->m_parent,t_node);
	}
	gc_assign(t_child->m_parent,t_node->m_parent);
	if((t_node->m_parent)!=0){
		if(t_node==t_node->m_parent->m_left){
			gc_assign(t_node->m_parent->m_left,t_child);
		}else{
			gc_assign(t_node->m_parent->m_right,t_child);
		}
	}else{
		gc_assign(m_root,t_child);
	}
	gc_assign(t_child->m_left,t_node);
	gc_assign(t_node->m_parent,t_child);
	return 0;
}
int c_Map4::p_RotateRight4(c_Node4* t_node){
	c_Node4* t_child=t_node->m_left;
	gc_assign(t_node->m_left,t_child->m_right);
	if((t_child->m_right)!=0){
		gc_assign(t_child->m_right->m_parent,t_node);
	}
	gc_assign(t_child->m_parent,t_node->m_parent);
	if((t_node->m_parent)!=0){
		if(t_node==t_node->m_parent->m_right){
			gc_assign(t_node->m_parent->m_right,t_child);
		}else{
			gc_assign(t_node->m_parent->m_left,t_child);
		}
	}else{
		gc_assign(m_root,t_child);
	}
	gc_assign(t_child->m_right,t_node);
	gc_assign(t_node->m_parent,t_child);
	return 0;
}
int c_Map4::p_InsertFixup4(c_Node4* t_node){
	while(((t_node->m_parent)!=0) && t_node->m_parent->m_color==-1 && ((t_node->m_parent->m_parent)!=0)){
		if(t_node->m_parent==t_node->m_parent->m_parent->m_left){
			c_Node4* t_uncle=t_node->m_parent->m_parent->m_right;
			if(((t_uncle)!=0) && t_uncle->m_color==-1){
				t_node->m_parent->m_color=1;
				t_uncle->m_color=1;
				t_uncle->m_parent->m_color=-1;
				t_node=t_uncle->m_parent;
			}else{
				if(t_node==t_node->m_parent->m_right){
					t_node=t_node->m_parent;
					p_RotateLeft5(t_node);
				}
				t_node->m_parent->m_color=1;
				t_node->m_parent->m_parent->m_color=-1;
				p_RotateRight4(t_node->m_parent->m_parent);
			}
		}else{
			c_Node4* t_uncle2=t_node->m_parent->m_parent->m_left;
			if(((t_uncle2)!=0) && t_uncle2->m_color==-1){
				t_node->m_parent->m_color=1;
				t_uncle2->m_color=1;
				t_uncle2->m_parent->m_color=-1;
				t_node=t_uncle2->m_parent;
			}else{
				if(t_node==t_node->m_parent->m_left){
					t_node=t_node->m_parent;
					p_RotateRight4(t_node);
				}
				t_node->m_parent->m_color=1;
				t_node->m_parent->m_parent->m_color=-1;
				p_RotateLeft5(t_node->m_parent->m_parent);
			}
		}
	}
	m_root->m_color=1;
	return 0;
}
bool c_Map4::p_Set12(String t_key,String t_value){
	c_Node4* t_node=m_root;
	c_Node4* t_parent=0;
	int t_cmp=0;
	while((t_node)!=0){
		t_parent=t_node;
		t_cmp=p_Compare6(t_key,t_node->m_key);
		if(t_cmp>0){
			t_node=t_node->m_right;
		}else{
			if(t_cmp<0){
				t_node=t_node->m_left;
			}else{
				t_node->m_value=t_value;
				return false;
			}
		}
	}
	t_node=(new c_Node4)->m_new(t_key,t_value,-1,t_parent);
	if((t_parent)!=0){
		if(t_cmp>0){
			gc_assign(t_parent->m_right,t_node);
		}else{
			gc_assign(t_parent->m_left,t_node);
		}
		p_InsertFixup4(t_node);
	}else{
		gc_assign(m_root,t_node);
	}
	return true;
}
c_Node4* c_Map4::p_FindNode3(String t_key){
	c_Node4* t_node=m_root;
	while((t_node)!=0){
		int t_cmp=p_Compare6(t_key,t_node->m_key);
		if(t_cmp>0){
			t_node=t_node->m_right;
		}else{
			if(t_cmp<0){
				t_node=t_node->m_left;
			}else{
				return t_node;
			}
		}
	}
	return t_node;
}
String c_Map4::p_Get3(String t_key){
	c_Node4* t_node=p_FindNode3(t_key);
	if((t_node)!=0){
		return t_node->m_value;
	}
	return String();
}
bool c_Map4::p_Contains3(String t_key){
	return p_FindNode3(t_key)!=0;
}
int c_Map4::p_Count(){
	if((m_root)!=0){
		return m_root->p_Count2(0);
	}
	return 0;
}
c_MapKeys* c_Map4::p_Keys(){
	return (new c_MapKeys)->m_new(this);
}
c_Node4* c_Map4::p_FirstNode(){
	if(!((m_root)!=0)){
		return 0;
	}
	c_Node4* t_node=m_root;
	while((t_node->m_left)!=0){
		t_node=t_node->m_left;
	}
	return t_node;
}
int c_Map4::p_Clear(){
	m_root=0;
	return 0;
}
bool c_Map4::p_IsEmpty(){
	return m_root==0;
}
bool c_Map4::p_Add6(String t_key,String t_value){
	c_Node4* t_node=m_root;
	c_Node4* t_parent=0;
	int t_cmp=0;
	while((t_node)!=0){
		t_parent=t_node;
		t_cmp=p_Compare6(t_key,t_node->m_key);
		if(t_cmp>0){
			t_node=t_node->m_right;
		}else{
			if(t_cmp<0){
				t_node=t_node->m_left;
			}else{
				return false;
			}
		}
	}
	t_node=(new c_Node4)->m_new(t_key,t_value,-1,t_parent);
	if((t_parent)!=0){
		if(t_cmp>0){
			gc_assign(t_parent->m_right,t_node);
		}else{
			gc_assign(t_parent->m_left,t_node);
		}
		p_InsertFixup4(t_node);
	}else{
		gc_assign(m_root,t_node);
	}
	return true;
}
bool c_Map4::p_Update5(String t_key,String t_value){
	c_Node4* t_node=p_FindNode3(t_key);
	if((t_node)!=0){
		t_node->m_value=t_value;
		return true;
	}
	return false;
}
int c_Map4::p_DeleteFixup4(c_Node4* t_node,c_Node4* t_parent){
	while(t_node!=m_root && (!((t_node)!=0) || t_node->m_color==1)){
		if(t_node==t_parent->m_left){
			c_Node4* t_sib=t_parent->m_right;
			if(t_sib->m_color==-1){
				t_sib->m_color=1;
				t_parent->m_color=-1;
				p_RotateLeft5(t_parent);
				t_sib=t_parent->m_right;
			}
			if((!((t_sib->m_left)!=0) || t_sib->m_left->m_color==1) && (!((t_sib->m_right)!=0) || t_sib->m_right->m_color==1)){
				t_sib->m_color=-1;
				t_node=t_parent;
				t_parent=t_parent->m_parent;
			}else{
				if(!((t_sib->m_right)!=0) || t_sib->m_right->m_color==1){
					t_sib->m_left->m_color=1;
					t_sib->m_color=-1;
					p_RotateRight4(t_sib);
					t_sib=t_parent->m_right;
				}
				t_sib->m_color=t_parent->m_color;
				t_parent->m_color=1;
				t_sib->m_right->m_color=1;
				p_RotateLeft5(t_parent);
				t_node=m_root;
			}
		}else{
			c_Node4* t_sib2=t_parent->m_left;
			if(t_sib2->m_color==-1){
				t_sib2->m_color=1;
				t_parent->m_color=-1;
				p_RotateRight4(t_parent);
				t_sib2=t_parent->m_left;
			}
			if((!((t_sib2->m_right)!=0) || t_sib2->m_right->m_color==1) && (!((t_sib2->m_left)!=0) || t_sib2->m_left->m_color==1)){
				t_sib2->m_color=-1;
				t_node=t_parent;
				t_parent=t_parent->m_parent;
			}else{
				if(!((t_sib2->m_left)!=0) || t_sib2->m_left->m_color==1){
					t_sib2->m_right->m_color=1;
					t_sib2->m_color=-1;
					p_RotateLeft5(t_sib2);
					t_sib2=t_parent->m_left;
				}
				t_sib2->m_color=t_parent->m_color;
				t_parent->m_color=1;
				t_sib2->m_left->m_color=1;
				p_RotateRight4(t_parent);
				t_node=m_root;
			}
		}
	}
	if((t_node)!=0){
		t_node->m_color=1;
	}
	return 0;
}
int c_Map4::p_RemoveNode4(c_Node4* t_node){
	c_Node4* t_splice=0;
	c_Node4* t_child=0;
	if(!((t_node->m_left)!=0)){
		t_splice=t_node;
		t_child=t_node->m_right;
	}else{
		if(!((t_node->m_right)!=0)){
			t_splice=t_node;
			t_child=t_node->m_left;
		}else{
			t_splice=t_node->m_left;
			while((t_splice->m_right)!=0){
				t_splice=t_splice->m_right;
			}
			t_child=t_splice->m_left;
			t_node->m_key=t_splice->m_key;
			t_node->m_value=t_splice->m_value;
		}
	}
	c_Node4* t_parent=t_splice->m_parent;
	if((t_child)!=0){
		gc_assign(t_child->m_parent,t_parent);
	}
	if(!((t_parent)!=0)){
		gc_assign(m_root,t_child);
		return 0;
	}
	if(t_splice==t_parent->m_left){
		gc_assign(t_parent->m_left,t_child);
	}else{
		gc_assign(t_parent->m_right,t_child);
	}
	if(t_splice->m_color==1){
		p_DeleteFixup4(t_child,t_parent);
	}
	return 0;
}
int c_Map4::p_Remove4(String t_key){
	c_Node4* t_node=p_FindNode3(t_key);
	if(!((t_node)!=0)){
		return 0;
	}
	p_RemoveNode4(t_node);
	return 1;
}
c_MapValues4* c_Map4::p_Values(){
	return (new c_MapValues4)->m_new(this);
}
c_NodeEnumerator4* c_Map4::p_ObjectEnumerator(){
	return (new c_NodeEnumerator4)->m_new(p_FirstNode());
}
bool c_Map4::p_Insert10(String t_key,String t_value){
	return p_Set12(t_key,t_value);
}
String c_Map4::p_ValueForKey3(String t_key){
	return p_Get3(t_key);
}
c_Node4* c_Map4::p_LastNode(){
	if(!((m_root)!=0)){
		return 0;
	}
	c_Node4* t_node=m_root;
	while((t_node->m_right)!=0){
		t_node=t_node->m_right;
	}
	return t_node;
}
void c_Map4::mark(){
	Object::mark();
	gc_mark_q(m_root);
}
c_StringMap2::c_StringMap2(){
}
c_StringMap2* c_StringMap2::m_new(){
	c_Map4::m_new();
	return this;
}
int c_StringMap2::p_Compare6(String t_lhs,String t_rhs){
	return t_lhs.Compare(t_rhs);
}
void c_StringMap2::mark(){
	c_Map4::mark();
}
c_Node4::c_Node4(){
	m_key=String();
	m_right=0;
	m_left=0;
	m_value=String();
	m_color=0;
	m_parent=0;
}
c_Node4* c_Node4::m_new(String t_key,String t_value,int t_color,c_Node4* t_parent){
	this->m_key=t_key;
	this->m_value=t_value;
	this->m_color=t_color;
	gc_assign(this->m_parent,t_parent);
	return this;
}
c_Node4* c_Node4::m_new2(){
	return this;
}
int c_Node4::p_Count2(int t_n){
	if((m_left)!=0){
		t_n=m_left->p_Count2(t_n);
	}
	if((m_right)!=0){
		t_n=m_right->p_Count2(t_n);
	}
	return t_n+1;
}
c_Node4* c_Node4::p_NextNode(){
	c_Node4* t_node=0;
	if((m_right)!=0){
		t_node=m_right;
		while((t_node->m_left)!=0){
			t_node=t_node->m_left;
		}
		return t_node;
	}
	t_node=this;
	c_Node4* t_parent=this->m_parent;
	while(((t_parent)!=0) && t_node==t_parent->m_right){
		t_node=t_parent;
		t_parent=t_parent->m_parent;
	}
	return t_parent;
}
String c_Node4::p_Key(){
	return m_key;
}
String c_Node4::p_Value(){
	return m_value;
}
c_Node4* c_Node4::p_PrevNode(){
	c_Node4* t_node=0;
	if((m_left)!=0){
		t_node=m_left;
		while((t_node->m_right)!=0){
			t_node=t_node->m_right;
		}
		return t_node;
	}
	t_node=this;
	c_Node4* t_parent=this->m_parent;
	while(((t_parent)!=0) && t_node==t_parent->m_left){
		t_node=t_parent;
		t_parent=t_parent->m_parent;
	}
	return t_parent;
}
c_Node4* c_Node4::p_Copy2(c_Node4* t_parent){
	c_Node4* t_t=(new c_Node4)->m_new(m_key,m_value,m_color,t_parent);
	if((m_left)!=0){
		gc_assign(t_t->m_left,m_left->p_Copy2(t_t));
	}
	if((m_right)!=0){
		gc_assign(t_t->m_right,m_right->p_Copy2(t_t));
	}
	return t_t;
}
void c_Node4::mark(){
	Object::mark();
	gc_mark_q(m_right);
	gc_mark_q(m_left);
	gc_mark_q(m_parent);
}
c_MapKeys::c_MapKeys(){
	m_map=0;
}
c_MapKeys* c_MapKeys::m_new(c_Map4* t_map){
	gc_assign(this->m_map,t_map);
	return this;
}
c_MapKeys* c_MapKeys::m_new2(){
	return this;
}
c_KeyEnumerator* c_MapKeys::p_ObjectEnumerator(){
	return (new c_KeyEnumerator)->m_new(m_map->p_FirstNode());
}
void c_MapKeys::mark(){
	Object::mark();
	gc_mark_q(m_map);
}
c_KeyEnumerator::c_KeyEnumerator(){
	m_node=0;
}
c_KeyEnumerator* c_KeyEnumerator::m_new(c_Node4* t_node){
	gc_assign(this->m_node,t_node);
	return this;
}
c_KeyEnumerator* c_KeyEnumerator::m_new2(){
	return this;
}
bool c_KeyEnumerator::p_HasNext(){
	return m_node!=0;
}
String c_KeyEnumerator::p_NextObject(){
	c_Node4* t_t=m_node;
	gc_assign(m_node,m_node->p_NextNode());
	return t_t->m_key;
}
void c_KeyEnumerator::mark(){
	Object::mark();
	gc_mark_q(m_node);
}
int bb_graphics_SetMatrix(Float t_ix,Float t_iy,Float t_jx,Float t_jy,Float t_tx,Float t_ty){
	bb_graphics_context->m_ix=t_ix;
	bb_graphics_context->m_iy=t_iy;
	bb_graphics_context->m_jx=t_jx;
	bb_graphics_context->m_jy=t_jy;
	bb_graphics_context->m_tx=t_tx;
	bb_graphics_context->m_ty=t_ty;
	bb_graphics_context->m_tformed=((t_ix!=FLOAT(1.0) || t_iy!=FLOAT(0.0) || t_jx!=FLOAT(0.0) || t_jy!=FLOAT(1.0) || t_tx!=FLOAT(0.0) || t_ty!=FLOAT(0.0))?1:0);
	bb_graphics_context->m_matDirty=1;
	return 0;
}
int bb_graphics_SetMatrix2(Array<Float > t_m){
	bb_graphics_SetMatrix(t_m[0],t_m[1],t_m[2],t_m[3],t_m[4],t_m[5]);
	return 0;
}
int bb_graphics_Transform(Float t_ix,Float t_iy,Float t_jx,Float t_jy,Float t_tx,Float t_ty){
	Float t_ix2=t_ix*bb_graphics_context->m_ix+t_iy*bb_graphics_context->m_jx;
	Float t_iy2=t_ix*bb_graphics_context->m_iy+t_iy*bb_graphics_context->m_jy;
	Float t_jx2=t_jx*bb_graphics_context->m_ix+t_jy*bb_graphics_context->m_jx;
	Float t_jy2=t_jx*bb_graphics_context->m_iy+t_jy*bb_graphics_context->m_jy;
	Float t_tx2=t_tx*bb_graphics_context->m_ix+t_ty*bb_graphics_context->m_jx+bb_graphics_context->m_tx;
	Float t_ty2=t_tx*bb_graphics_context->m_iy+t_ty*bb_graphics_context->m_jy+bb_graphics_context->m_ty;
	bb_graphics_SetMatrix(t_ix2,t_iy2,t_jx2,t_jy2,t_tx2,t_ty2);
	return 0;
}
int bb_graphics_Transform2(Array<Float > t_m){
	bb_graphics_Transform(t_m[0],t_m[1],t_m[2],t_m[3],t_m[4],t_m[5]);
	return 0;
}
int bb_graphics_Translate(Float t_x,Float t_y){
	bb_graphics_Transform(FLOAT(1.0),FLOAT(0.0),FLOAT(0.0),FLOAT(1.0),t_x,t_y);
	return 0;
}
void bb_functions_TranslateV(c_Vec2* t_vector){
	bb_graphics_Translate(t_vector->m_x,t_vector->m_y);
}
int bb_graphics_Rotate(Float t_angle){
	bb_graphics_Transform((Float)cos((t_angle)*D2R),-(Float)sin((t_angle)*D2R),(Float)sin((t_angle)*D2R),(Float)cos((t_angle)*D2R),FLOAT(0.0),FLOAT(0.0));
	return 0;
}
int bb_graphics_Scale(Float t_x,Float t_y){
	bb_graphics_Transform(t_x,FLOAT(0.0),FLOAT(0.0),t_y,FLOAT(0.0),FLOAT(0.0));
	return 0;
}
void bb_functions_ScaleV(c_Vec2* t_vector){
	bb_graphics_Scale(t_vector->m_x,t_vector->m_y);
}
c_VShape::c_VShape(){
	m_color=(new c_Color)->m_new4();
	m_renderOutline=false;
}
void c_VShape::p_Render(){
	m_color->p_Use();
	bb_graphics_PushMatrix();
	bb_graphics_Translate(m_position->m_x,m_position->m_y);
	bb_graphics_Rotate(m_rotation);
	bb_graphics_Scale(m_scale->m_x,m_scale->m_y);
	if(!m_renderOutline){
		p_Draw();
	}else{
		p_DrawOutline();
	}
	bb_graphics_PopMatrix();
}
bool c_VShape::p_CollidesWith3(c_VShape* t_shape){
	if((dynamic_cast<c_VRect*>(t_shape))!=0){
		return p_CollidesWith(dynamic_cast<c_VRect*>(t_shape));
	}else{
		if((dynamic_cast<c_VCircle*>(t_shape))!=0){
			return p_CollidesWith2(dynamic_cast<c_VCircle*>(t_shape));
		}
	}
	return false;
}
c_VShape* c_VShape::m_new(){
	c_VEntity::m_new();
	return this;
}
void c_VShape::mark(){
	c_VEntity::mark();
	gc_mark_q(m_color);
}
int bb_graphics_PushMatrix(){
	int t_sp=bb_graphics_context->m_matrixSp;
	bb_graphics_context->m_matrixStack[t_sp+0]=bb_graphics_context->m_ix;
	bb_graphics_context->m_matrixStack[t_sp+1]=bb_graphics_context->m_iy;
	bb_graphics_context->m_matrixStack[t_sp+2]=bb_graphics_context->m_jx;
	bb_graphics_context->m_matrixStack[t_sp+3]=bb_graphics_context->m_jy;
	bb_graphics_context->m_matrixStack[t_sp+4]=bb_graphics_context->m_tx;
	bb_graphics_context->m_matrixStack[t_sp+5]=bb_graphics_context->m_ty;
	bb_graphics_context->m_matrixSp=t_sp+6;
	return 0;
}
int bb_graphics_PopMatrix(){
	int t_sp=bb_graphics_context->m_matrixSp-6;
	bb_graphics_SetMatrix(bb_graphics_context->m_matrixStack[t_sp+0],bb_graphics_context->m_matrixStack[t_sp+1],bb_graphics_context->m_matrixStack[t_sp+2],bb_graphics_context->m_matrixStack[t_sp+3],bb_graphics_context->m_matrixStack[t_sp+4],bb_graphics_context->m_matrixStack[t_sp+5]);
	bb_graphics_context->m_matrixSp=t_sp;
	return 0;
}
c_VRect::c_VRect(){
	m_size=0;
}
c_VRect* c_VRect::m_new(Float t_x,Float t_y,Float t_w,Float t_h){
	c_VShape::m_new();
	m_position->p_Set10(t_x,t_y);
	gc_assign(m_size,(new c_Vec2)->m_new(t_w,t_h));
	return this;
}
c_VRect* c_VRect::m_new2(c_Vec2* t_a,c_Vec2* t_b,c_Vec2* t_c,c_Vec2* t_d){
	c_VShape::m_new();
	m_position->p_Set11(t_a);
	gc_assign(m_size,(new c_Vec2)->m_new3());
	m_size->m_x=t_b->m_x-t_a->m_x;
	m_size->m_y=t_c->m_y-t_a->m_y;
	return this;
}
c_VRect* c_VRect::m_new3(){
	c_VShape::m_new();
	bb_functions2_NoDefaultConstructorError(String(L"VRect",5));
	return this;
}
c_VRect* c_VRect::p_Copy(){
	return (new c_VRect)->m_new(m_position->m_x,m_position->m_y,m_size->m_x,m_size->m_y);
}
Float c_VRect::p_Radius(){
	return bb_math2_RectRadius(m_size->m_x,m_size->m_y);
}
void c_VRect::p_Draw(){
	bb_graphics_DrawRect(FLOAT(0.0),FLOAT(0.0),m_size->m_x,m_size->m_y);
}
void c_VRect::p_DrawOutline(){
	bb_functions_DrawRectOutline(FLOAT(0.0),FLOAT(0.0),m_size->m_x,m_size->m_y);
}
c_Vec2* c_VRect::p_TopLeft(){
	return (new c_Vec2)->m_new(m_position->m_x,m_position->m_y);
}
c_Vec2* c_VRect::p_TopRightUntransformed(){
	return (new c_Vec2)->m_new(m_position->m_x+m_size->m_x,m_position->m_y);
}
c_Vec2* c_VRect::p_TopRight(){
	c_Vec2* t_point=p_TopRightUntransformed();
	if(m_rotation==FLOAT(0.0)){
		return t_point;
	}
	return bb_math2_RotatePoint(t_point,-m_rotation,m_position);
}
c_Vec2* c_VRect::p_BottomLeftUntransformed(){
	return (new c_Vec2)->m_new(m_position->m_x,m_position->m_y+m_size->m_y);
}
c_Vec2* c_VRect::p_BottomLeft(){
	c_Vec2* t_point=p_BottomLeftUntransformed();
	if(m_rotation==FLOAT(0.0)){
		return t_point;
	}
	return bb_math2_RotatePoint(t_point,-m_rotation,m_position);
}
c_Vec2* c_VRect::p_BottomRightUntransformed(){
	return (new c_Vec2)->m_new(m_position->m_x+m_size->m_x,m_position->m_y+m_size->m_y);
}
c_Vec2* c_VRect::p_BottomRight(){
	c_Vec2* t_point=p_BottomRightUntransformed();
	if(m_rotation==FLOAT(0.0)){
		return t_point;
	}
	return bb_math2_RotatePoint(t_point,-m_rotation,m_position);
}
c_Vec2* c_VRect::p_TopLeftUntransformed(){
	return (new c_Vec2)->m_new(m_position->m_x,m_position->m_y);
}
bool c_VRect::p_PointInside(c_Vec2* t_point){
	if(m_rotation==FLOAT(0.0)){
		return bb_math2_PointInRect(t_point->m_x,t_point->m_y,m_position->m_x,m_position->m_y,m_size->m_x,m_size->m_y);
	}
	c_Vec2* t_transformedPoint=bb_math2_RotatePoint(t_point,m_rotation,m_position);
	return bb_math2_PointInRect(t_transformedPoint->m_x,t_transformedPoint->m_y,m_position->m_x,m_position->m_y,m_size->m_x,m_size->m_y);
}
bool c_VRect::p_CollidesWith(c_VRect* t_with){
	if(m_rotation==FLOAT(0.0) && t_with->m_rotation==FLOAT(0.0)){
		return bb_math2_RectsOverlap(m_position->m_x,m_position->m_y,m_size->m_x,m_size->m_y,t_with->m_position->m_x,t_with->m_position->m_y,t_with->m_size->m_x,t_with->m_size->m_y);
	}
	c_Vec2* t_a1=p_TopLeft();
	c_Vec2* t_b1=p_TopRight();
	c_Vec2* t_c1=p_BottomLeft();
	c_Vec2* t_d1=p_BottomRight();
	c_Vec2* t_a2=t_with->p_TopLeft();
	c_Vec2* t_b2=t_with->p_TopRight();
	c_Vec2* t_c2=t_with->p_BottomLeft();
	c_Vec2* t_d2=t_with->p_BottomRight();
	if(bb_math2_LinesIntersect(t_a1,t_b1,t_a2,t_b2)){
		return true;
	}
	if(bb_math2_LinesIntersect(t_b1,t_d1,t_a2,t_b2)){
		return true;
	}
	if(bb_math2_LinesIntersect(t_c1,t_d1,t_a2,t_b2)){
		return true;
	}
	if(bb_math2_LinesIntersect(t_a1,t_c1,t_a2,t_b2)){
		return true;
	}
	if(bb_math2_LinesIntersect(t_a1,t_b1,t_b2,t_d2)){
		return true;
	}
	if(bb_math2_LinesIntersect(t_b1,t_d1,t_b2,t_d2)){
		return true;
	}
	if(bb_math2_LinesIntersect(t_c1,t_d1,t_b2,t_d2)){
		return true;
	}
	if(bb_math2_LinesIntersect(t_a1,t_c1,t_b2,t_d2)){
		return true;
	}
	if(bb_math2_LinesIntersect(t_a1,t_b1,t_c2,t_d2)){
		return true;
	}
	if(bb_math2_LinesIntersect(t_b1,t_d1,t_c2,t_d2)){
		return true;
	}
	if(bb_math2_LinesIntersect(t_c1,t_d1,t_c2,t_d2)){
		return true;
	}
	if(bb_math2_LinesIntersect(t_a1,t_c1,t_c2,t_d2)){
		return true;
	}
	if(bb_math2_LinesIntersect(t_a1,t_b1,t_a2,t_c2)){
		return true;
	}
	if(bb_math2_LinesIntersect(t_b1,t_d1,t_a2,t_c2)){
		return true;
	}
	if(bb_math2_LinesIntersect(t_c1,t_d1,t_a2,t_c2)){
		return true;
	}
	if(bb_math2_LinesIntersect(t_a1,t_c1,t_a2,t_c2)){
		return true;
	}
	t_b1=bb_math2_RotatePoint(t_b1,m_rotation,t_a1);
	t_c1=bb_math2_RotatePoint(t_c1,m_rotation,t_a1);
	t_d1=bb_math2_RotatePoint(t_d1,m_rotation,t_a1);
	c_VRect* t_helpRect=(new c_VRect)->m_new2(t_a1,t_b1,t_c1,t_d1);
	t_a2=bb_math2_RotatePoint(t_a2,m_rotation,t_a1);
	t_b2=bb_math2_RotatePoint(t_b2,m_rotation,t_a1);
	t_c2=bb_math2_RotatePoint(t_c2,m_rotation,t_a1);
	t_d2=bb_math2_RotatePoint(t_d2,m_rotation,t_a1);
	if(t_helpRect->p_PointInside(t_a2)){
		return true;
	}
	if(t_helpRect->p_PointInside(t_b2)){
		return true;
	}
	if(t_helpRect->p_PointInside(t_c2)){
		return true;
	}
	if(t_helpRect->p_PointInside(t_d2)){
		return true;
	}
	return false;
}
bool c_VRect::p_CollidesWith2(c_VCircle* t_circle){
	if(p_PointInside(t_circle->m_position)){
		return true;
	}
	c_Vec2* t_topLeft=p_TopLeftUntransformed();
	c_Vec2* t_topRight=p_TopRightUntransformed();
	c_Vec2* t_bottomLeft=p_BottomLeftUntransformed();
	c_Vec2* t_bottomRight=p_BottomRightUntransformed();
	c_VCircle* t_rotatedCircle=t_circle->p_Copy();
	gc_assign(t_rotatedCircle->m_position,bb_math2_RotatePoint(t_circle->m_position,m_rotation,m_position));
	return t_rotatedCircle->p_CollidesWithLine(t_topLeft,t_topRight) || t_rotatedCircle->p_CollidesWithLine(t_topRight,t_bottomRight) || t_rotatedCircle->p_CollidesWithLine(t_topLeft,t_bottomLeft) || t_rotatedCircle->p_CollidesWithLine(t_bottomLeft,t_bottomRight);
}
void c_VRect::mark(){
	c_VShape::mark();
	gc_mark_q(m_size);
}
c_VCircle::c_VCircle(){
	m_radius=FLOAT(.0);
}
c_VCircle* c_VCircle::m_new(Float t_x,Float t_y,Float t_radius){
	c_VShape::m_new();
	m_position->p_Set10(t_x,t_y);
	this->m_radius=t_radius;
	return this;
}
c_VCircle* c_VCircle::m_new2(){
	c_VShape::m_new();
	bb_functions2_NoDefaultConstructorError(String(L"VCircle",7));
	return this;
}
c_VCircle* c_VCircle::p_Copy(){
	return (new c_VCircle)->m_new(m_position->m_x,m_position->m_y,m_radius);
}
bool c_VCircle::p_CollidesWithLine(c_Vec2* t_lineStart,c_Vec2* t_lineEnd){
	return bb_math2_PerpendicularDistance(this->m_position,t_lineStart,t_lineEnd)<=m_radius+FLOAT(1.0);
}
void c_VCircle::p_Draw(){
	bb_graphics_DrawCircle(FLOAT(0.0),FLOAT(0.0),m_radius);
}
void c_VCircle::p_DrawOutline(){
	bb_functions_DrawCircleOutline(FLOAT(0.0),FLOAT(0.0),m_radius,-1);
}
Float c_VCircle::p_Radius(){
	return m_radius;
}
bool c_VCircle::p_PointInside(c_Vec2* t_point){
	return bb_math2_PointInCircle(t_point->m_x,t_point->m_y,m_position->m_x,m_position->m_y,m_radius);
}
bool c_VCircle::p_CollidesWith2(c_VCircle* t_with){
	return bb_math2_CirclesOverlap(m_position->m_x,m_position->m_y,m_radius,t_with->m_position->m_x,t_with->m_position->m_y,t_with->m_radius);
}
bool c_VCircle::p_CollidesWith(c_VRect* t_with){
	return t_with->p_CollidesWith2(this);
}
void c_VCircle::mark(){
	c_VShape::mark();
}
Float bb_math2_RectRadius(Float t_w,Float t_h){
	Float t_x2=t_w/FLOAT(2.0);
	Float t_y2=t_h/FLOAT(2.0);
	return (Float)sqrt(t_x2*t_x2+t_y2*t_y2);
}
int bb_graphics_DrawRect(Float t_x,Float t_y,Float t_w,Float t_h){
	bb_graphics_context->p_Validate();
	bb_graphics_renderDevice->DrawRect(t_x,t_y,t_w,t_h);
	return 0;
}
int bb_graphics_DrawLine(Float t_x1,Float t_y1,Float t_x2,Float t_y2){
	bb_graphics_context->p_Validate();
	bb_graphics_renderDevice->DrawLine(t_x1,t_y1,t_x2,t_y2);
	return 0;
}
void bb_functions_DrawRectOutline(Float t_x,Float t_y,Float t_width,Float t_height){
	bb_graphics_DrawLine(t_x,t_y,t_x+t_width,t_y);
	bb_graphics_DrawLine(t_x+t_width,t_y,t_x+t_width,t_y+t_height);
	bb_graphics_DrawLine(t_x+t_width,t_y+t_height,t_x,t_y+t_height);
	bb_graphics_DrawLine(t_x,t_y+t_height,t_x,t_y);
}
c_Vec2* bb_math2_RotatePoint(c_Vec2* t_point,Float t_angle,c_Vec2* t_origin){
	c_Vec2* t_p=(new c_Vec2)->m_new3();
	t_p->m_x=(Float)cos((t_angle)*D2R)*(t_point->m_x-t_origin->m_x)-(Float)sin((t_angle)*D2R)*(t_point->m_y-t_origin->m_y)+t_origin->m_x;
	t_p->m_y=(Float)sin((t_angle)*D2R)*(t_point->m_x-t_origin->m_x)+(Float)cos((t_angle)*D2R)*(t_point->m_y-t_origin->m_y)+t_origin->m_y;
	return t_p;
}
bool bb_math2_PointInRect(Float t_x,Float t_y,Float t_rectX,Float t_rectY,Float t_rectW,Float t_rectH){
	return t_x>t_rectX && t_x<t_rectX+t_rectW && t_y>t_rectY && t_y<t_rectY+t_rectH;
}
bool bb_math2_RectsOverlap(Float t_x1,Float t_y1,Float t_w1,Float t_h1,Float t_x2,Float t_y2,Float t_w2,Float t_h2){
	if(t_x1>t_x2+t_w2 || t_x1+t_w1<t_x2){
		return false;
	}
	if(t_y1>t_y2+t_h2 || t_y1+t_h1<t_y2){
		return false;
	}
	return true;
}
bool bb_math2_LinesIntersect(c_Vec2* t_a,c_Vec2* t_b,c_Vec2* t_c,c_Vec2* t_d){
	Float t_denominator=(t_b->m_x-t_a->m_x)*(t_d->m_y-t_c->m_y)-(t_b->m_y-t_a->m_y)*(t_d->m_x-t_c->m_x);
	Float t_numerator1=(t_a->m_y-t_c->m_y)*(t_d->m_x-t_c->m_x)-(t_a->m_x-t_c->m_x)*(t_d->m_y-t_c->m_y);
	Float t_numerator2=(t_a->m_y-t_c->m_y)*(t_b->m_x-t_a->m_x)-(t_a->m_x-t_c->m_x)*(t_b->m_y-t_a->m_y);
	if(t_denominator==FLOAT(0.0)){
		return t_numerator1==FLOAT(0.0) && t_numerator2==FLOAT(0.0);
	}
	Float t_r=t_numerator1/t_denominator;
	Float t_s=t_numerator2/t_denominator;
	return t_r>=FLOAT(0.0) && t_r<=FLOAT(1.0) && (t_s>=FLOAT(0.0) && t_s<=FLOAT(1.0));
}
Float bb_math2_PerpendicularDistance(c_Vec2* t_point,c_Vec2* t_lineStart,c_Vec2* t_lineEnd){
	Float t_A=t_point->m_x-t_lineStart->m_x;
	Float t_B=t_point->m_y-t_lineStart->m_y;
	Float t_C=t_lineEnd->m_x-t_lineStart->m_x;
	Float t_D=t_lineEnd->m_y-t_lineStart->m_y;
	Float t_dot=t_A*t_C+t_B*t_D;
	Float t_len_sq=t_C*t_C+t_D*t_D;
	Float t_param=t_dot/t_len_sq;
	Float t_xx=FLOAT(.0);
	Float t_yy=FLOAT(.0);
	if(t_param<FLOAT(0.0) || t_lineStart->m_x==t_lineEnd->m_x && t_lineStart->m_y==t_lineEnd->m_y){
		t_xx=t_lineStart->m_x;
		t_yy=t_lineStart->m_y;
	}else{
		if(t_param>FLOAT(1.0)){
			t_xx=t_lineEnd->m_x;
			t_yy=t_lineEnd->m_y;
		}else{
			t_xx=t_lineStart->m_x+t_param*t_C;
			t_yy=t_lineStart->m_y+t_param*t_D;
		}
	}
	Float t_dx=t_point->m_x-t_xx;
	Float t_dy=t_point->m_y-t_yy;
	return (Float)sqrt(t_dx*t_dx+t_dy*t_dy);
}
int bb_graphics_DrawCircle(Float t_x,Float t_y,Float t_r){
	bb_graphics_context->p_Validate();
	bb_graphics_renderDevice->DrawOval(t_x-t_r,t_y-t_r,t_r*FLOAT(2.0),t_r*FLOAT(2.0));
	return 0;
}
void bb_functions_DrawCircleOutline(Float t_x,Float t_y,Float t_radius,int t_detail){
	if(t_detail<0){
		t_detail=int(t_radius);
	}else{
		if(t_detail<3){
			t_detail=3;
		}else{
			if(t_detail>1024){
				t_detail=1024;
			}
		}
	}
	Float t_angleStep=FLOAT(360.0)/Float(t_detail);
	Float t_angle=FLOAT(.0);
	Float t_offsetX=FLOAT(.0);
	Float t_offsetY=FLOAT(.0);
	bool t_first=true;
	Float t_firstX=FLOAT(.0);
	Float t_firstY=FLOAT(.0);
	Float t_thisX=FLOAT(.0);
	Float t_thisY=FLOAT(.0);
	Float t_lastX=FLOAT(.0);
	Float t_lastY=FLOAT(.0);
	for(int t_vertIndex=0;t_vertIndex<t_detail;t_vertIndex=t_vertIndex+1){
		t_offsetX=(Float)sin((t_angle)*D2R)*t_radius;
		t_offsetY=(Float)cos((t_angle)*D2R)*t_radius;
		if(t_first){
			t_first=false;
			t_firstX=t_x+t_offsetX;
			t_firstY=t_y+t_offsetY;
			t_lastX=t_firstX;
			t_lastY=t_firstY;
		}else{
			t_thisX=t_x+t_offsetX;
			t_thisY=t_y+t_offsetY;
			bb_graphics_DrawLine(t_lastX,t_lastY,t_thisX,t_thisY);
			t_lastX=t_thisX;
			t_lastY=t_thisY;
		}
		t_angle+=t_angleStep;
	}
	bb_graphics_DrawLine(t_lastX,t_lastY,t_firstX,t_firstY);
}
Float bb_math2_DistanceOfPoints(Float t_x1,Float t_y1,Float t_x2,Float t_y2){
	return (Float)sqrt((t_x1-t_x2)*(t_x1-t_x2)+(t_y1-t_y2)*(t_y1-t_y2));
}
bool bb_math2_PointInCircle(Float t_pointX,Float t_pointY,Float t_circleX,Float t_circleY,Float t_radius){
	return bb_math2_DistanceOfPoints(t_pointX,t_pointY,t_circleX,t_circleY)<=t_radius;
}
bool bb_math2_CirclesOverlap(Float t_x1,Float t_y1,Float t_r1,Float t_x2,Float t_y2,Float t_r2){
	return (Float)sqrt((t_x1-t_x2)*(t_x1-t_x2)+(t_y1-t_y2)*(t_y1-t_y2))-t_r1-t_r2<FLOAT(0.0);
}
c_VSprite::c_VSprite(){
	m_color=(new c_Color)->m_new4();
	m_hidden=false;
	m_flipX=false;
	m_flipY=false;
	m_image=0;
	m_imagePath=String();
}
void c_VSprite::p_SetImage(String t_path,int t_flags){
	gc_assign(m_image,c_ImageCache::m_GetImage(t_path,t_flags));
	if(m_image==0){
		throw (new c_Exception)->m_new2(String(L"Sprite: Could not load image at path: ",38)+t_path);
	}
	m_imagePath=t_path;
}
c_VSprite* c_VSprite::m_new(String t_imagePath,Float t_x,Float t_y){
	c_VEntity::m_new();
	p_SetImage(t_imagePath,1);
	m_position->p_Set10(t_x,t_y);
	return this;
}
c_VSprite* c_VSprite::m_new2(){
	c_VEntity::m_new();
	return this;
}
c_VSprite* c_VSprite::p_Copy(){
	c_VSprite* t_sprite=(new c_VSprite)->m_new(m_imagePath,FLOAT(0.0),FLOAT(0.0));
	t_sprite->m_position->p_Set11(m_position);
	t_sprite->m_scale->p_Set11(m_scale);
	t_sprite->m_rotation=m_rotation;
	t_sprite->m_color->p_Set8(m_color);
	t_sprite->m_hidden=m_hidden;
	t_sprite->m_flipX=m_flipX;
	t_sprite->m_flipY=m_flipY;
	c_StringMap2* t_attributeMap=this->p_GetAttributeMap();
	if((t_attributeMap)!=0){
		t_sprite->p_SetAttributeMap(t_attributeMap);
	}
	return t_sprite;
}
void c_VSprite::p_SetHandle(Float t_x,Float t_y){
	m_image->p_SetHandle(t_x,t_y);
}
void c_VSprite::p_SetColor(Float t_r,Float t_g,Float t_b){
	m_color->p_Set7(t_r,t_g,t_b,FLOAT(1.0));
}
Float c_VSprite::p_Width(){
	return Float(m_image->p_Width())*m_scale->m_x;
}
Float c_VSprite::p_Height(){
	return Float(m_image->p_Height())*m_scale->m_y;
}
String c_VSprite::p_ImagePath(){
	return m_imagePath;
}
Float c_VSprite::p_HandleX(){
	return m_image->p_HandleX();
}
Float c_VSprite::p_HandleY(){
	return m_image->p_HandleY();
}
void c_VSprite::p_Alpha2(Float t_alpha){
	m_color->p_Alpha2(t_alpha);
}
Float c_VSprite::p_Alpha(){
	return m_color->p_Alpha();
}
void c_VSprite::p_DrawImage(){
	if((m_image)!=0){
		bb_graphics_DrawImage(m_image,FLOAT(0.0),FLOAT(0.0),0);
	}
}
void c_VSprite::p_Render(){
	if(m_hidden || m_color->p_Alpha()<FLOAT(0.001) || m_image==0){
		return;
	}
	m_color->p_Use();
	Float t_x=FLOAT(1.0);
	Float t_y=FLOAT(1.0);
	if(m_flipX){
		t_x=FLOAT(-1.0);
	}
	if(m_flipY){
		t_y=FLOAT(-1.0);
	}
	bb_graphics_PushMatrix();
	bb_graphics_Translate(m_position->m_x,m_position->m_y);
	bb_graphics_Rotate(m_rotation);
	if(m_scale->m_x*t_x!=FLOAT(1.0) || m_scale->m_y*t_y!=FLOAT(1.0)){
		bb_graphics_Scale(m_scale->m_x*t_x,m_scale->m_y*t_y);
	}
	this->p_DrawImage();
	bb_graphics_PopMatrix();
}
void c_VSprite::p_Update4(Float t_dt){
}
void c_VSprite::mark(){
	c_VEntity::mark();
	gc_mark_q(m_color);
	gc_mark_q(m_image);
}
c_Image::c_Image(){
	m_surface=0;
	m_width=0;
	m_height=0;
	m_frames=Array<c_Frame* >();
	m_flags=0;
	m_tx=FLOAT(.0);
	m_ty=FLOAT(.0);
	m_source=0;
}
int c_Image::m_DefaultFlags;
c_Image* c_Image::m_new(){
	return this;
}
int c_Image::p_SetHandle(Float t_tx,Float t_ty){
	this->m_tx=t_tx;
	this->m_ty=t_ty;
	this->m_flags=this->m_flags&-2;
	return 0;
}
int c_Image::p_ApplyFlags(int t_iflags){
	m_flags=t_iflags;
	if((m_flags&2)!=0){
		Array<c_Frame* > t_=m_frames;
		int t_2=0;
		while(t_2<t_.Length()){
			c_Frame* t_f=t_[t_2];
			t_2=t_2+1;
			t_f->m_x+=1;
		}
		m_width-=2;
	}
	if((m_flags&4)!=0){
		Array<c_Frame* > t_3=m_frames;
		int t_4=0;
		while(t_4<t_3.Length()){
			c_Frame* t_f2=t_3[t_4];
			t_4=t_4+1;
			t_f2->m_y+=1;
		}
		m_height-=2;
	}
	if((m_flags&1)!=0){
		p_SetHandle(Float(m_width)/FLOAT(2.0),Float(m_height)/FLOAT(2.0));
	}
	if(m_frames.Length()==1 && m_frames[0]->m_x==0 && m_frames[0]->m_y==0 && m_width==m_surface->Width() && m_height==m_surface->Height()){
		m_flags|=65536;
	}
	return 0;
}
c_Image* c_Image::p_Init(gxtkSurface* t_surf,int t_nframes,int t_iflags){
	gc_assign(m_surface,t_surf);
	m_width=m_surface->Width()/t_nframes;
	m_height=m_surface->Height();
	gc_assign(m_frames,Array<c_Frame* >(t_nframes));
	for(int t_i=0;t_i<t_nframes;t_i=t_i+1){
		gc_assign(m_frames[t_i],(new c_Frame)->m_new(t_i*m_width,0));
	}
	p_ApplyFlags(t_iflags);
	return this;
}
c_Image* c_Image::p_Init2(gxtkSurface* t_surf,int t_x,int t_y,int t_iwidth,int t_iheight,int t_nframes,int t_iflags,c_Image* t_src,int t_srcx,int t_srcy,int t_srcw,int t_srch){
	gc_assign(m_surface,t_surf);
	gc_assign(m_source,t_src);
	m_width=t_iwidth;
	m_height=t_iheight;
	gc_assign(m_frames,Array<c_Frame* >(t_nframes));
	int t_ix=t_x;
	int t_iy=t_y;
	for(int t_i=0;t_i<t_nframes;t_i=t_i+1){
		if(t_ix+m_width>t_srcw){
			t_ix=0;
			t_iy+=m_height;
		}
		if(t_ix+m_width>t_srcw || t_iy+m_height>t_srch){
			bbError(String(L"Image frame outside surface",27));
		}
		gc_assign(m_frames[t_i],(new c_Frame)->m_new(t_ix+t_srcx,t_iy+t_srcy));
		t_ix+=m_width;
	}
	p_ApplyFlags(t_iflags);
	return this;
}
int c_Image::p_Width(){
	return m_width;
}
int c_Image::p_Height(){
	return m_height;
}
Float c_Image::p_HandleX(){
	return m_tx;
}
Float c_Image::p_HandleY(){
	return m_ty;
}
void c_Image::mark(){
	Object::mark();
	gc_mark_q(m_surface);
	gc_mark_q(m_frames);
	gc_mark_q(m_source);
}
c_ImageCache::c_ImageCache(){
}
c_StringMap3* c_ImageCache::m_ImageCache;
c_Image* c_ImageCache::m_GetImage(String t_path,int t_flags){
	if(m_ImageCache->p_Contains3(t_path)){
		return m_ImageCache->p_Get3(t_path);
	}else{
		c_Image* t_image=bb_graphics_LoadImage(t_path,1,t_flags);
		if(!((t_image)!=0)){
			return 0;
		}
		m_ImageCache->p_Set13(t_path,t_image);
		return t_image;
	}
}
void c_ImageCache::mark(){
	Object::mark();
}
c_Map5::c_Map5(){
	m_root=0;
}
c_Map5* c_Map5::m_new(){
	return this;
}
c_Node5* c_Map5::p_FindNode3(String t_key){
	c_Node5* t_node=m_root;
	while((t_node)!=0){
		int t_cmp=p_Compare6(t_key,t_node->m_key);
		if(t_cmp>0){
			t_node=t_node->m_right;
		}else{
			if(t_cmp<0){
				t_node=t_node->m_left;
			}else{
				return t_node;
			}
		}
	}
	return t_node;
}
bool c_Map5::p_Contains3(String t_key){
	return p_FindNode3(t_key)!=0;
}
c_Image* c_Map5::p_Get3(String t_key){
	c_Node5* t_node=p_FindNode3(t_key);
	if((t_node)!=0){
		return t_node->m_value;
	}
	return 0;
}
int c_Map5::p_RotateLeft6(c_Node5* t_node){
	c_Node5* t_child=t_node->m_right;
	gc_assign(t_node->m_right,t_child->m_left);
	if((t_child->m_left)!=0){
		gc_assign(t_child->m_left->m_parent,t_node);
	}
	gc_assign(t_child->m_parent,t_node->m_parent);
	if((t_node->m_parent)!=0){
		if(t_node==t_node->m_parent->m_left){
			gc_assign(t_node->m_parent->m_left,t_child);
		}else{
			gc_assign(t_node->m_parent->m_right,t_child);
		}
	}else{
		gc_assign(m_root,t_child);
	}
	gc_assign(t_child->m_left,t_node);
	gc_assign(t_node->m_parent,t_child);
	return 0;
}
int c_Map5::p_RotateRight5(c_Node5* t_node){
	c_Node5* t_child=t_node->m_left;
	gc_assign(t_node->m_left,t_child->m_right);
	if((t_child->m_right)!=0){
		gc_assign(t_child->m_right->m_parent,t_node);
	}
	gc_assign(t_child->m_parent,t_node->m_parent);
	if((t_node->m_parent)!=0){
		if(t_node==t_node->m_parent->m_right){
			gc_assign(t_node->m_parent->m_right,t_child);
		}else{
			gc_assign(t_node->m_parent->m_left,t_child);
		}
	}else{
		gc_assign(m_root,t_child);
	}
	gc_assign(t_child->m_right,t_node);
	gc_assign(t_node->m_parent,t_child);
	return 0;
}
int c_Map5::p_InsertFixup5(c_Node5* t_node){
	while(((t_node->m_parent)!=0) && t_node->m_parent->m_color==-1 && ((t_node->m_parent->m_parent)!=0)){
		if(t_node->m_parent==t_node->m_parent->m_parent->m_left){
			c_Node5* t_uncle=t_node->m_parent->m_parent->m_right;
			if(((t_uncle)!=0) && t_uncle->m_color==-1){
				t_node->m_parent->m_color=1;
				t_uncle->m_color=1;
				t_uncle->m_parent->m_color=-1;
				t_node=t_uncle->m_parent;
			}else{
				if(t_node==t_node->m_parent->m_right){
					t_node=t_node->m_parent;
					p_RotateLeft6(t_node);
				}
				t_node->m_parent->m_color=1;
				t_node->m_parent->m_parent->m_color=-1;
				p_RotateRight5(t_node->m_parent->m_parent);
			}
		}else{
			c_Node5* t_uncle2=t_node->m_parent->m_parent->m_left;
			if(((t_uncle2)!=0) && t_uncle2->m_color==-1){
				t_node->m_parent->m_color=1;
				t_uncle2->m_color=1;
				t_uncle2->m_parent->m_color=-1;
				t_node=t_uncle2->m_parent;
			}else{
				if(t_node==t_node->m_parent->m_left){
					t_node=t_node->m_parent;
					p_RotateRight5(t_node);
				}
				t_node->m_parent->m_color=1;
				t_node->m_parent->m_parent->m_color=-1;
				p_RotateLeft6(t_node->m_parent->m_parent);
			}
		}
	}
	m_root->m_color=1;
	return 0;
}
bool c_Map5::p_Set13(String t_key,c_Image* t_value){
	c_Node5* t_node=m_root;
	c_Node5* t_parent=0;
	int t_cmp=0;
	while((t_node)!=0){
		t_parent=t_node;
		t_cmp=p_Compare6(t_key,t_node->m_key);
		if(t_cmp>0){
			t_node=t_node->m_right;
		}else{
			if(t_cmp<0){
				t_node=t_node->m_left;
			}else{
				gc_assign(t_node->m_value,t_value);
				return false;
			}
		}
	}
	t_node=(new c_Node5)->m_new(t_key,t_value,-1,t_parent);
	if((t_parent)!=0){
		if(t_cmp>0){
			gc_assign(t_parent->m_right,t_node);
		}else{
			gc_assign(t_parent->m_left,t_node);
		}
		p_InsertFixup5(t_node);
	}else{
		gc_assign(m_root,t_node);
	}
	return true;
}
int c_Map5::p_Clear(){
	m_root=0;
	return 0;
}
int c_Map5::p_Count(){
	if((m_root)!=0){
		return m_root->p_Count2(0);
	}
	return 0;
}
bool c_Map5::p_IsEmpty(){
	return m_root==0;
}
bool c_Map5::p_Add7(String t_key,c_Image* t_value){
	c_Node5* t_node=m_root;
	c_Node5* t_parent=0;
	int t_cmp=0;
	while((t_node)!=0){
		t_parent=t_node;
		t_cmp=p_Compare6(t_key,t_node->m_key);
		if(t_cmp>0){
			t_node=t_node->m_right;
		}else{
			if(t_cmp<0){
				t_node=t_node->m_left;
			}else{
				return false;
			}
		}
	}
	t_node=(new c_Node5)->m_new(t_key,t_value,-1,t_parent);
	if((t_parent)!=0){
		if(t_cmp>0){
			gc_assign(t_parent->m_right,t_node);
		}else{
			gc_assign(t_parent->m_left,t_node);
		}
		p_InsertFixup5(t_node);
	}else{
		gc_assign(m_root,t_node);
	}
	return true;
}
bool c_Map5::p_Update6(String t_key,c_Image* t_value){
	c_Node5* t_node=p_FindNode3(t_key);
	if((t_node)!=0){
		gc_assign(t_node->m_value,t_value);
		return true;
	}
	return false;
}
int c_Map5::p_DeleteFixup5(c_Node5* t_node,c_Node5* t_parent){
	while(t_node!=m_root && (!((t_node)!=0) || t_node->m_color==1)){
		if(t_node==t_parent->m_left){
			c_Node5* t_sib=t_parent->m_right;
			if(t_sib->m_color==-1){
				t_sib->m_color=1;
				t_parent->m_color=-1;
				p_RotateLeft6(t_parent);
				t_sib=t_parent->m_right;
			}
			if((!((t_sib->m_left)!=0) || t_sib->m_left->m_color==1) && (!((t_sib->m_right)!=0) || t_sib->m_right->m_color==1)){
				t_sib->m_color=-1;
				t_node=t_parent;
				t_parent=t_parent->m_parent;
			}else{
				if(!((t_sib->m_right)!=0) || t_sib->m_right->m_color==1){
					t_sib->m_left->m_color=1;
					t_sib->m_color=-1;
					p_RotateRight5(t_sib);
					t_sib=t_parent->m_right;
				}
				t_sib->m_color=t_parent->m_color;
				t_parent->m_color=1;
				t_sib->m_right->m_color=1;
				p_RotateLeft6(t_parent);
				t_node=m_root;
			}
		}else{
			c_Node5* t_sib2=t_parent->m_left;
			if(t_sib2->m_color==-1){
				t_sib2->m_color=1;
				t_parent->m_color=-1;
				p_RotateRight5(t_parent);
				t_sib2=t_parent->m_left;
			}
			if((!((t_sib2->m_right)!=0) || t_sib2->m_right->m_color==1) && (!((t_sib2->m_left)!=0) || t_sib2->m_left->m_color==1)){
				t_sib2->m_color=-1;
				t_node=t_parent;
				t_parent=t_parent->m_parent;
			}else{
				if(!((t_sib2->m_left)!=0) || t_sib2->m_left->m_color==1){
					t_sib2->m_right->m_color=1;
					t_sib2->m_color=-1;
					p_RotateLeft6(t_sib2);
					t_sib2=t_parent->m_left;
				}
				t_sib2->m_color=t_parent->m_color;
				t_parent->m_color=1;
				t_sib2->m_left->m_color=1;
				p_RotateRight5(t_parent);
				t_node=m_root;
			}
		}
	}
	if((t_node)!=0){
		t_node->m_color=1;
	}
	return 0;
}
int c_Map5::p_RemoveNode5(c_Node5* t_node){
	c_Node5* t_splice=0;
	c_Node5* t_child=0;
	if(!((t_node->m_left)!=0)){
		t_splice=t_node;
		t_child=t_node->m_right;
	}else{
		if(!((t_node->m_right)!=0)){
			t_splice=t_node;
			t_child=t_node->m_left;
		}else{
			t_splice=t_node->m_left;
			while((t_splice->m_right)!=0){
				t_splice=t_splice->m_right;
			}
			t_child=t_splice->m_left;
			t_node->m_key=t_splice->m_key;
			gc_assign(t_node->m_value,t_splice->m_value);
		}
	}
	c_Node5* t_parent=t_splice->m_parent;
	if((t_child)!=0){
		gc_assign(t_child->m_parent,t_parent);
	}
	if(!((t_parent)!=0)){
		gc_assign(m_root,t_child);
		return 0;
	}
	if(t_splice==t_parent->m_left){
		gc_assign(t_parent->m_left,t_child);
	}else{
		gc_assign(t_parent->m_right,t_child);
	}
	if(t_splice->m_color==1){
		p_DeleteFixup5(t_child,t_parent);
	}
	return 0;
}
int c_Map5::p_Remove4(String t_key){
	c_Node5* t_node=p_FindNode3(t_key);
	if(!((t_node)!=0)){
		return 0;
	}
	p_RemoveNode5(t_node);
	return 1;
}
c_MapKeys5* c_Map5::p_Keys(){
	return (new c_MapKeys5)->m_new(this);
}
c_MapValues5* c_Map5::p_Values(){
	return (new c_MapValues5)->m_new(this);
}
c_Node5* c_Map5::p_FirstNode(){
	if(!((m_root)!=0)){
		return 0;
	}
	c_Node5* t_node=m_root;
	while((t_node->m_left)!=0){
		t_node=t_node->m_left;
	}
	return t_node;
}
c_NodeEnumerator5* c_Map5::p_ObjectEnumerator(){
	return (new c_NodeEnumerator5)->m_new(p_FirstNode());
}
bool c_Map5::p_Insert11(String t_key,c_Image* t_value){
	return p_Set13(t_key,t_value);
}
c_Image* c_Map5::p_ValueForKey3(String t_key){
	return p_Get3(t_key);
}
c_Node5* c_Map5::p_LastNode(){
	if(!((m_root)!=0)){
		return 0;
	}
	c_Node5* t_node=m_root;
	while((t_node->m_right)!=0){
		t_node=t_node->m_right;
	}
	return t_node;
}
void c_Map5::mark(){
	Object::mark();
	gc_mark_q(m_root);
}
c_StringMap3::c_StringMap3(){
}
c_StringMap3* c_StringMap3::m_new(){
	c_Map5::m_new();
	return this;
}
int c_StringMap3::p_Compare6(String t_lhs,String t_rhs){
	return t_lhs.Compare(t_rhs);
}
void c_StringMap3::mark(){
	c_Map5::mark();
}
c_Node5::c_Node5(){
	m_key=String();
	m_right=0;
	m_left=0;
	m_value=0;
	m_color=0;
	m_parent=0;
}
c_Node5* c_Node5::m_new(String t_key,c_Image* t_value,int t_color,c_Node5* t_parent){
	this->m_key=t_key;
	gc_assign(this->m_value,t_value);
	this->m_color=t_color;
	gc_assign(this->m_parent,t_parent);
	return this;
}
c_Node5* c_Node5::m_new2(){
	return this;
}
int c_Node5::p_Count2(int t_n){
	if((m_left)!=0){
		t_n=m_left->p_Count2(t_n);
	}
	if((m_right)!=0){
		t_n=m_right->p_Count2(t_n);
	}
	return t_n+1;
}
String c_Node5::p_Key(){
	return m_key;
}
c_Image* c_Node5::p_Value(){
	return m_value;
}
c_Node5* c_Node5::p_NextNode(){
	c_Node5* t_node=0;
	if((m_right)!=0){
		t_node=m_right;
		while((t_node->m_left)!=0){
			t_node=t_node->m_left;
		}
		return t_node;
	}
	t_node=this;
	c_Node5* t_parent=this->m_parent;
	while(((t_parent)!=0) && t_node==t_parent->m_right){
		t_node=t_parent;
		t_parent=t_parent->m_parent;
	}
	return t_parent;
}
c_Node5* c_Node5::p_PrevNode(){
	c_Node5* t_node=0;
	if((m_left)!=0){
		t_node=m_left;
		while((t_node->m_right)!=0){
			t_node=t_node->m_right;
		}
		return t_node;
	}
	t_node=this;
	c_Node5* t_parent=this->m_parent;
	while(((t_parent)!=0) && t_node==t_parent->m_left){
		t_node=t_parent;
		t_parent=t_parent->m_parent;
	}
	return t_parent;
}
c_Node5* c_Node5::p_Copy3(c_Node5* t_parent){
	c_Node5* t_t=(new c_Node5)->m_new(m_key,m_value,m_color,t_parent);
	if((m_left)!=0){
		gc_assign(t_t->m_left,m_left->p_Copy3(t_t));
	}
	if((m_right)!=0){
		gc_assign(t_t->m_right,m_right->p_Copy3(t_t));
	}
	return t_t;
}
void c_Node5::mark(){
	Object::mark();
	gc_mark_q(m_right);
	gc_mark_q(m_left);
	gc_mark_q(m_value);
	gc_mark_q(m_parent);
}
String bb_data_FixDataPath(String t_path){
	int t_i=t_path.Find(String(L":/",2),0);
	if(t_i!=-1 && t_path.Find(String(L"/",1),0)==t_i+1){
		return t_path;
	}
	if(t_path.StartsWith(String(L"./",2)) || t_path.StartsWith(String(L"/",1))){
		return t_path;
	}
	return String(L"monkey://data/",14)+t_path;
}
gxtkGraphics* bb_graphics_device;
c_Frame::c_Frame(){
	m_x=0;
	m_y=0;
}
c_Frame* c_Frame::m_new(int t_x,int t_y){
	this->m_x=t_x;
	this->m_y=t_y;
	return this;
}
c_Frame* c_Frame::m_new2(){
	return this;
}
void c_Frame::mark(){
	Object::mark();
}
c_Image* bb_graphics_LoadImage(String t_path,int t_frameCount,int t_flags){
	gxtkSurface* t_surf=bb_graphics_device->LoadSurface(bb_data_FixDataPath(t_path));
	if((t_surf)!=0){
		return ((new c_Image)->m_new())->p_Init(t_surf,t_frameCount,t_flags);
	}
	return 0;
}
c_Image* bb_graphics_LoadImage2(String t_path,int t_frameWidth,int t_frameHeight,int t_frameCount,int t_flags){
	gxtkSurface* t_surf=bb_graphics_device->LoadSurface(bb_data_FixDataPath(t_path));
	if((t_surf)!=0){
		return ((new c_Image)->m_new())->p_Init2(t_surf,0,0,t_frameWidth,t_frameHeight,t_frameCount,t_flags,0,0,0,t_surf->Width(),t_surf->Height());
	}
	return 0;
}
c_Exception::c_Exception(){
	m_message=String();
}
c_Exception* c_Exception::m_new(){
	bb_functions2_NoDefaultConstructorError(String(L"Exception",9));
	return this;
}
c_Exception* c_Exception::m_new2(String t_message){
	this->m_message=t_message;
	return this;
}
String c_Exception::p_ToString(){
	return m_message;
}
void c_Exception::mark(){
	ThrowableObject::mark();
}
int bb_graphics_DrawImage(c_Image* t_image,Float t_x,Float t_y,int t_frame){
	c_Frame* t_f=t_image->m_frames[t_frame];
	bb_graphics_context->p_Validate();
	if((t_image->m_flags&65536)!=0){
		bb_graphics_renderDevice->DrawSurface(t_image->m_surface,t_x-t_image->m_tx,t_y-t_image->m_ty);
	}else{
		bb_graphics_renderDevice->DrawSurface2(t_image->m_surface,t_x-t_image->m_tx,t_y-t_image->m_ty,t_f->m_x,t_f->m_y,t_image->m_width,t_image->m_height);
	}
	return 0;
}
int bb_graphics_DrawImage2(c_Image* t_image,Float t_x,Float t_y,Float t_rotation,Float t_scaleX,Float t_scaleY,int t_frame){
	c_Frame* t_f=t_image->m_frames[t_frame];
	bb_graphics_PushMatrix();
	bb_graphics_Translate(t_x,t_y);
	bb_graphics_Rotate(t_rotation);
	bb_graphics_Scale(t_scaleX,t_scaleY);
	bb_graphics_Translate(-t_image->m_tx,-t_image->m_ty);
	bb_graphics_context->p_Validate();
	if((t_image->m_flags&65536)!=0){
		bb_graphics_renderDevice->DrawSurface(t_image->m_surface,FLOAT(0.0),FLOAT(0.0));
	}else{
		bb_graphics_renderDevice->DrawSurface2(t_image->m_surface,FLOAT(0.0),FLOAT(0.0),t_f->m_x,t_f->m_y,t_image->m_width,t_image->m_height);
	}
	bb_graphics_PopMatrix();
	return 0;
}
c_Enumerator2::c_Enumerator2(){
	m__deque=0;
	m__index=-1;
}
c_Enumerator2* c_Enumerator2::m_new(c_Deque* t_deque){
	gc_assign(m__deque,t_deque);
	return this;
}
c_Enumerator2* c_Enumerator2::m_new2(){
	return this;
}
bool c_Enumerator2::p_HasNext(){
	return m__index<m__deque->p_Length()-1;
}
int c_Enumerator2::p_NextObject(){
	m__index+=1;
	return m__deque->p_Get(m__index);
}
void c_Enumerator2::mark(){
	Object::mark();
	gc_mark_q(m__deque);
}
c_Enumerator3::c_Enumerator3(){
	m__deque=0;
	m__index=-1;
}
c_Enumerator3* c_Enumerator3::m_new(c_Deque2* t_deque){
	gc_assign(m__deque,t_deque);
	return this;
}
c_Enumerator3* c_Enumerator3::m_new2(){
	return this;
}
bool c_Enumerator3::p_HasNext(){
	return m__index<m__deque->p_Length()-1;
}
Float c_Enumerator3::p_NextObject(){
	m__index+=1;
	return m__deque->p_Get(m__index);
}
void c_Enumerator3::mark(){
	Object::mark();
	gc_mark_q(m__deque);
}
c_Enumerator4::c_Enumerator4(){
	m__deque=0;
	m__index=-1;
}
c_Enumerator4* c_Enumerator4::m_new(c_Deque3* t_deque){
	gc_assign(m__deque,t_deque);
	return this;
}
c_Enumerator4* c_Enumerator4::m_new2(){
	return this;
}
bool c_Enumerator4::p_HasNext(){
	return m__index<m__deque->p_Length()-1;
}
String c_Enumerator4::p_NextObject(){
	m__index+=1;
	return m__deque->p_Get(m__index);
}
void c_Enumerator4::mark(){
	Object::mark();
	gc_mark_q(m__deque);
}
c_Enumerator5::c_Enumerator5(){
	m__list=0;
	m__curr=0;
}
c_Enumerator5* c_Enumerator5::m_new(c_List* t_list){
	gc_assign(m__list,t_list);
	gc_assign(m__curr,t_list->m__head->m__succ);
	return this;
}
c_Enumerator5* c_Enumerator5::m_new2(){
	return this;
}
bool c_Enumerator5::p_HasNext(){
	while(m__curr->m__succ->m__pred!=m__curr){
		gc_assign(m__curr,m__curr->m__succ);
	}
	return m__curr!=m__list->m__head;
}
int c_Enumerator5::p_NextObject(){
	int t_data=m__curr->m__data;
	gc_assign(m__curr,m__curr->m__succ);
	return t_data;
}
void c_Enumerator5::mark(){
	Object::mark();
	gc_mark_q(m__list);
	gc_mark_q(m__curr);
}
c_BackwardsList::c_BackwardsList(){
	m__list=0;
}
c_BackwardsList* c_BackwardsList::m_new(c_List* t_list){
	gc_assign(m__list,t_list);
	return this;
}
c_BackwardsList* c_BackwardsList::m_new2(){
	return this;
}
c_BackwardsEnumerator* c_BackwardsList::p_ObjectEnumerator(){
	return (new c_BackwardsEnumerator)->m_new(m__list);
}
void c_BackwardsList::mark(){
	Object::mark();
	gc_mark_q(m__list);
}
c_Enumerator6::c_Enumerator6(){
	m__list=0;
	m__curr=0;
}
c_Enumerator6* c_Enumerator6::m_new(c_List2* t_list){
	gc_assign(m__list,t_list);
	gc_assign(m__curr,t_list->m__head->m__succ);
	return this;
}
c_Enumerator6* c_Enumerator6::m_new2(){
	return this;
}
bool c_Enumerator6::p_HasNext(){
	while(m__curr->m__succ->m__pred!=m__curr){
		gc_assign(m__curr,m__curr->m__succ);
	}
	return m__curr!=m__list->m__head;
}
Float c_Enumerator6::p_NextObject(){
	Float t_data=m__curr->m__data;
	gc_assign(m__curr,m__curr->m__succ);
	return t_data;
}
void c_Enumerator6::mark(){
	Object::mark();
	gc_mark_q(m__list);
	gc_mark_q(m__curr);
}
c_BackwardsList2::c_BackwardsList2(){
	m__list=0;
}
c_BackwardsList2* c_BackwardsList2::m_new(c_List2* t_list){
	gc_assign(m__list,t_list);
	return this;
}
c_BackwardsList2* c_BackwardsList2::m_new2(){
	return this;
}
c_BackwardsEnumerator2* c_BackwardsList2::p_ObjectEnumerator(){
	return (new c_BackwardsEnumerator2)->m_new(m__list);
}
void c_BackwardsList2::mark(){
	Object::mark();
	gc_mark_q(m__list);
}
c_BackwardsList3::c_BackwardsList3(){
	m__list=0;
}
c_BackwardsList3* c_BackwardsList3::m_new(c_List3* t_list){
	gc_assign(m__list,t_list);
	return this;
}
c_BackwardsList3* c_BackwardsList3::m_new2(){
	return this;
}
c_BackwardsEnumerator3* c_BackwardsList3::p_ObjectEnumerator(){
	return (new c_BackwardsEnumerator3)->m_new(m__list);
}
void c_BackwardsList3::mark(){
	Object::mark();
	gc_mark_q(m__list);
}
c_BackwardsEnumerator::c_BackwardsEnumerator(){
	m__list=0;
	m__curr=0;
}
c_BackwardsEnumerator* c_BackwardsEnumerator::m_new(c_List* t_list){
	gc_assign(m__list,t_list);
	gc_assign(m__curr,t_list->m__head->m__pred);
	return this;
}
c_BackwardsEnumerator* c_BackwardsEnumerator::m_new2(){
	return this;
}
bool c_BackwardsEnumerator::p_HasNext(){
	while(m__curr->m__pred->m__succ!=m__curr){
		gc_assign(m__curr,m__curr->m__pred);
	}
	return m__curr!=m__list->m__head;
}
int c_BackwardsEnumerator::p_NextObject(){
	int t_data=m__curr->m__data;
	gc_assign(m__curr,m__curr->m__pred);
	return t_data;
}
void c_BackwardsEnumerator::mark(){
	Object::mark();
	gc_mark_q(m__list);
	gc_mark_q(m__curr);
}
c_BackwardsEnumerator2::c_BackwardsEnumerator2(){
	m__list=0;
	m__curr=0;
}
c_BackwardsEnumerator2* c_BackwardsEnumerator2::m_new(c_List2* t_list){
	gc_assign(m__list,t_list);
	gc_assign(m__curr,t_list->m__head->m__pred);
	return this;
}
c_BackwardsEnumerator2* c_BackwardsEnumerator2::m_new2(){
	return this;
}
bool c_BackwardsEnumerator2::p_HasNext(){
	while(m__curr->m__pred->m__succ!=m__curr){
		gc_assign(m__curr,m__curr->m__pred);
	}
	return m__curr!=m__list->m__head;
}
Float c_BackwardsEnumerator2::p_NextObject(){
	Float t_data=m__curr->m__data;
	gc_assign(m__curr,m__curr->m__pred);
	return t_data;
}
void c_BackwardsEnumerator2::mark(){
	Object::mark();
	gc_mark_q(m__list);
	gc_mark_q(m__curr);
}
c_BackwardsEnumerator3::c_BackwardsEnumerator3(){
	m__list=0;
	m__curr=0;
}
c_BackwardsEnumerator3* c_BackwardsEnumerator3::m_new(c_List3* t_list){
	gc_assign(m__list,t_list);
	gc_assign(m__curr,t_list->m__head->m__pred);
	return this;
}
c_BackwardsEnumerator3* c_BackwardsEnumerator3::m_new2(){
	return this;
}
bool c_BackwardsEnumerator3::p_HasNext(){
	while(m__curr->m__pred->m__succ!=m__curr){
		gc_assign(m__curr,m__curr->m__pred);
	}
	return m__curr!=m__list->m__head;
}
String c_BackwardsEnumerator3::p_NextObject(){
	String t_data=m__curr->m__data;
	gc_assign(m__curr,m__curr->m__pred);
	return t_data;
}
void c_BackwardsEnumerator3::mark(){
	Object::mark();
	gc_mark_q(m__list);
	gc_mark_q(m__curr);
}
c_Node6::c_Node6(){
	m_left=0;
	m_right=0;
	m_key=0;
	m_value=0;
	m_color=0;
	m_parent=0;
}
int c_Node6::p_Count2(int t_n){
	if((m_left)!=0){
		t_n=m_left->p_Count2(t_n);
	}
	if((m_right)!=0){
		t_n=m_right->p_Count2(t_n);
	}
	return t_n+1;
}
c_Node6* c_Node6::m_new(int t_key,Object* t_value,int t_color,c_Node6* t_parent){
	this->m_key=t_key;
	gc_assign(this->m_value,t_value);
	this->m_color=t_color;
	gc_assign(this->m_parent,t_parent);
	return this;
}
c_Node6* c_Node6::m_new2(){
	return this;
}
int c_Node6::p_Key(){
	return m_key;
}
Object* c_Node6::p_Value(){
	return m_value;
}
c_Node6* c_Node6::p_NextNode(){
	c_Node6* t_node=0;
	if((m_right)!=0){
		t_node=m_right;
		while((t_node->m_left)!=0){
			t_node=t_node->m_left;
		}
		return t_node;
	}
	t_node=this;
	c_Node6* t_parent=this->m_parent;
	while(((t_parent)!=0) && t_node==t_parent->m_right){
		t_node=t_parent;
		t_parent=t_parent->m_parent;
	}
	return t_parent;
}
c_Node6* c_Node6::p_PrevNode(){
	c_Node6* t_node=0;
	if((m_left)!=0){
		t_node=m_left;
		while((t_node->m_right)!=0){
			t_node=t_node->m_right;
		}
		return t_node;
	}
	t_node=this;
	c_Node6* t_parent=this->m_parent;
	while(((t_parent)!=0) && t_node==t_parent->m_left){
		t_node=t_parent;
		t_parent=t_parent->m_parent;
	}
	return t_parent;
}
c_Node6* c_Node6::p_Copy4(c_Node6* t_parent){
	c_Node6* t_t=(new c_Node6)->m_new(m_key,m_value,m_color,t_parent);
	if((m_left)!=0){
		gc_assign(t_t->m_left,m_left->p_Copy4(t_t));
	}
	if((m_right)!=0){
		gc_assign(t_t->m_right,m_right->p_Copy4(t_t));
	}
	return t_t;
}
void c_Node6::mark(){
	Object::mark();
	gc_mark_q(m_left);
	gc_mark_q(m_right);
	gc_mark_q(m_value);
	gc_mark_q(m_parent);
}
c_MapKeys2::c_MapKeys2(){
	m_map=0;
}
c_MapKeys2* c_MapKeys2::m_new(c_Map* t_map){
	gc_assign(this->m_map,t_map);
	return this;
}
c_MapKeys2* c_MapKeys2::m_new2(){
	return this;
}
c_KeyEnumerator2* c_MapKeys2::p_ObjectEnumerator(){
	return (new c_KeyEnumerator2)->m_new(m_map->p_FirstNode());
}
void c_MapKeys2::mark(){
	Object::mark();
	gc_mark_q(m_map);
}
c_MapValues::c_MapValues(){
	m_map=0;
}
c_MapValues* c_MapValues::m_new(c_Map* t_map){
	gc_assign(this->m_map,t_map);
	return this;
}
c_MapValues* c_MapValues::m_new2(){
	return this;
}
c_ValueEnumerator* c_MapValues::p_ObjectEnumerator(){
	return (new c_ValueEnumerator)->m_new(m_map->p_FirstNode());
}
void c_MapValues::mark(){
	Object::mark();
	gc_mark_q(m_map);
}
c_NodeEnumerator::c_NodeEnumerator(){
	m_node=0;
}
c_NodeEnumerator* c_NodeEnumerator::m_new(c_Node6* t_node){
	gc_assign(this->m_node,t_node);
	return this;
}
c_NodeEnumerator* c_NodeEnumerator::m_new2(){
	return this;
}
bool c_NodeEnumerator::p_HasNext(){
	return m_node!=0;
}
c_Node6* c_NodeEnumerator::p_NextObject(){
	c_Node6* t_t=m_node;
	gc_assign(m_node,m_node->p_NextNode());
	return t_t;
}
void c_NodeEnumerator::mark(){
	Object::mark();
	gc_mark_q(m_node);
}
c_Node7::c_Node7(){
	m_left=0;
	m_right=0;
	m_key=0;
	m_value=0;
	m_color=0;
	m_parent=0;
}
int c_Node7::p_Count2(int t_n){
	if((m_left)!=0){
		t_n=m_left->p_Count2(t_n);
	}
	if((m_right)!=0){
		t_n=m_right->p_Count2(t_n);
	}
	return t_n+1;
}
c_Node7* c_Node7::m_new(Float t_key,Object* t_value,int t_color,c_Node7* t_parent){
	this->m_key=t_key;
	gc_assign(this->m_value,t_value);
	this->m_color=t_color;
	gc_assign(this->m_parent,t_parent);
	return this;
}
c_Node7* c_Node7::m_new2(){
	return this;
}
Float c_Node7::p_Key(){
	return m_key;
}
Object* c_Node7::p_Value(){
	return m_value;
}
c_Node7* c_Node7::p_NextNode(){
	c_Node7* t_node=0;
	if((m_right)!=0){
		t_node=m_right;
		while((t_node->m_left)!=0){
			t_node=t_node->m_left;
		}
		return t_node;
	}
	t_node=this;
	c_Node7* t_parent=this->m_parent;
	while(((t_parent)!=0) && t_node==t_parent->m_right){
		t_node=t_parent;
		t_parent=t_parent->m_parent;
	}
	return t_parent;
}
c_Node7* c_Node7::p_PrevNode(){
	c_Node7* t_node=0;
	if((m_left)!=0){
		t_node=m_left;
		while((t_node->m_right)!=0){
			t_node=t_node->m_right;
		}
		return t_node;
	}
	t_node=this;
	c_Node7* t_parent=this->m_parent;
	while(((t_parent)!=0) && t_node==t_parent->m_left){
		t_node=t_parent;
		t_parent=t_parent->m_parent;
	}
	return t_parent;
}
c_Node7* c_Node7::p_Copy5(c_Node7* t_parent){
	c_Node7* t_t=(new c_Node7)->m_new(m_key,m_value,m_color,t_parent);
	if((m_left)!=0){
		gc_assign(t_t->m_left,m_left->p_Copy5(t_t));
	}
	if((m_right)!=0){
		gc_assign(t_t->m_right,m_right->p_Copy5(t_t));
	}
	return t_t;
}
void c_Node7::mark(){
	Object::mark();
	gc_mark_q(m_left);
	gc_mark_q(m_right);
	gc_mark_q(m_value);
	gc_mark_q(m_parent);
}
c_MapKeys3::c_MapKeys3(){
	m_map=0;
}
c_MapKeys3* c_MapKeys3::m_new(c_Map2* t_map){
	gc_assign(this->m_map,t_map);
	return this;
}
c_MapKeys3* c_MapKeys3::m_new2(){
	return this;
}
c_KeyEnumerator3* c_MapKeys3::p_ObjectEnumerator(){
	return (new c_KeyEnumerator3)->m_new(m_map->p_FirstNode());
}
void c_MapKeys3::mark(){
	Object::mark();
	gc_mark_q(m_map);
}
c_MapValues2::c_MapValues2(){
	m_map=0;
}
c_MapValues2* c_MapValues2::m_new(c_Map2* t_map){
	gc_assign(this->m_map,t_map);
	return this;
}
c_MapValues2* c_MapValues2::m_new2(){
	return this;
}
c_ValueEnumerator2* c_MapValues2::p_ObjectEnumerator(){
	return (new c_ValueEnumerator2)->m_new(m_map->p_FirstNode());
}
void c_MapValues2::mark(){
	Object::mark();
	gc_mark_q(m_map);
}
c_NodeEnumerator2::c_NodeEnumerator2(){
	m_node=0;
}
c_NodeEnumerator2* c_NodeEnumerator2::m_new(c_Node7* t_node){
	gc_assign(this->m_node,t_node);
	return this;
}
c_NodeEnumerator2* c_NodeEnumerator2::m_new2(){
	return this;
}
bool c_NodeEnumerator2::p_HasNext(){
	return m_node!=0;
}
c_Node7* c_NodeEnumerator2::p_NextObject(){
	c_Node7* t_t=m_node;
	gc_assign(m_node,m_node->p_NextNode());
	return t_t;
}
void c_NodeEnumerator2::mark(){
	Object::mark();
	gc_mark_q(m_node);
}
c_Node8::c_Node8(){
	m_left=0;
	m_right=0;
	m_key=String();
	m_value=0;
	m_color=0;
	m_parent=0;
}
int c_Node8::p_Count2(int t_n){
	if((m_left)!=0){
		t_n=m_left->p_Count2(t_n);
	}
	if((m_right)!=0){
		t_n=m_right->p_Count2(t_n);
	}
	return t_n+1;
}
c_Node8* c_Node8::m_new(String t_key,Object* t_value,int t_color,c_Node8* t_parent){
	this->m_key=t_key;
	gc_assign(this->m_value,t_value);
	this->m_color=t_color;
	gc_assign(this->m_parent,t_parent);
	return this;
}
c_Node8* c_Node8::m_new2(){
	return this;
}
String c_Node8::p_Key(){
	return m_key;
}
Object* c_Node8::p_Value(){
	return m_value;
}
c_Node8* c_Node8::p_NextNode(){
	c_Node8* t_node=0;
	if((m_right)!=0){
		t_node=m_right;
		while((t_node->m_left)!=0){
			t_node=t_node->m_left;
		}
		return t_node;
	}
	t_node=this;
	c_Node8* t_parent=this->m_parent;
	while(((t_parent)!=0) && t_node==t_parent->m_right){
		t_node=t_parent;
		t_parent=t_parent->m_parent;
	}
	return t_parent;
}
c_Node8* c_Node8::p_PrevNode(){
	c_Node8* t_node=0;
	if((m_left)!=0){
		t_node=m_left;
		while((t_node->m_right)!=0){
			t_node=t_node->m_right;
		}
		return t_node;
	}
	t_node=this;
	c_Node8* t_parent=this->m_parent;
	while(((t_parent)!=0) && t_node==t_parent->m_left){
		t_node=t_parent;
		t_parent=t_parent->m_parent;
	}
	return t_parent;
}
c_Node8* c_Node8::p_Copy6(c_Node8* t_parent){
	c_Node8* t_t=(new c_Node8)->m_new(m_key,m_value,m_color,t_parent);
	if((m_left)!=0){
		gc_assign(t_t->m_left,m_left->p_Copy6(t_t));
	}
	if((m_right)!=0){
		gc_assign(t_t->m_right,m_right->p_Copy6(t_t));
	}
	return t_t;
}
void c_Node8::mark(){
	Object::mark();
	gc_mark_q(m_left);
	gc_mark_q(m_right);
	gc_mark_q(m_value);
	gc_mark_q(m_parent);
}
c_MapKeys4::c_MapKeys4(){
	m_map=0;
}
c_MapKeys4* c_MapKeys4::m_new(c_Map3* t_map){
	gc_assign(this->m_map,t_map);
	return this;
}
c_MapKeys4* c_MapKeys4::m_new2(){
	return this;
}
c_KeyEnumerator4* c_MapKeys4::p_ObjectEnumerator(){
	return (new c_KeyEnumerator4)->m_new(m_map->p_FirstNode());
}
void c_MapKeys4::mark(){
	Object::mark();
	gc_mark_q(m_map);
}
c_MapValues3::c_MapValues3(){
	m_map=0;
}
c_MapValues3* c_MapValues3::m_new(c_Map3* t_map){
	gc_assign(this->m_map,t_map);
	return this;
}
c_MapValues3* c_MapValues3::m_new2(){
	return this;
}
c_ValueEnumerator3* c_MapValues3::p_ObjectEnumerator(){
	return (new c_ValueEnumerator3)->m_new(m_map->p_FirstNode());
}
void c_MapValues3::mark(){
	Object::mark();
	gc_mark_q(m_map);
}
c_NodeEnumerator3::c_NodeEnumerator3(){
	m_node=0;
}
c_NodeEnumerator3* c_NodeEnumerator3::m_new(c_Node8* t_node){
	gc_assign(this->m_node,t_node);
	return this;
}
c_NodeEnumerator3* c_NodeEnumerator3::m_new2(){
	return this;
}
bool c_NodeEnumerator3::p_HasNext(){
	return m_node!=0;
}
c_Node8* c_NodeEnumerator3::p_NextObject(){
	c_Node8* t_t=m_node;
	gc_assign(m_node,m_node->p_NextNode());
	return t_t;
}
void c_NodeEnumerator3::mark(){
	Object::mark();
	gc_mark_q(m_node);
}
c_MapValues4::c_MapValues4(){
	m_map=0;
}
c_MapValues4* c_MapValues4::m_new(c_Map4* t_map){
	gc_assign(this->m_map,t_map);
	return this;
}
c_MapValues4* c_MapValues4::m_new2(){
	return this;
}
c_ValueEnumerator4* c_MapValues4::p_ObjectEnumerator(){
	return (new c_ValueEnumerator4)->m_new(m_map->p_FirstNode());
}
void c_MapValues4::mark(){
	Object::mark();
	gc_mark_q(m_map);
}
c_NodeEnumerator4::c_NodeEnumerator4(){
	m_node=0;
}
c_NodeEnumerator4* c_NodeEnumerator4::m_new(c_Node4* t_node){
	gc_assign(this->m_node,t_node);
	return this;
}
c_NodeEnumerator4* c_NodeEnumerator4::m_new2(){
	return this;
}
bool c_NodeEnumerator4::p_HasNext(){
	return m_node!=0;
}
c_Node4* c_NodeEnumerator4::p_NextObject(){
	c_Node4* t_t=m_node;
	gc_assign(m_node,m_node->p_NextNode());
	return t_t;
}
void c_NodeEnumerator4::mark(){
	Object::mark();
	gc_mark_q(m_node);
}
c_MapKeys5::c_MapKeys5(){
	m_map=0;
}
c_MapKeys5* c_MapKeys5::m_new(c_Map5* t_map){
	gc_assign(this->m_map,t_map);
	return this;
}
c_MapKeys5* c_MapKeys5::m_new2(){
	return this;
}
c_KeyEnumerator5* c_MapKeys5::p_ObjectEnumerator(){
	return (new c_KeyEnumerator5)->m_new(m_map->p_FirstNode());
}
void c_MapKeys5::mark(){
	Object::mark();
	gc_mark_q(m_map);
}
c_MapValues5::c_MapValues5(){
	m_map=0;
}
c_MapValues5* c_MapValues5::m_new(c_Map5* t_map){
	gc_assign(this->m_map,t_map);
	return this;
}
c_MapValues5* c_MapValues5::m_new2(){
	return this;
}
c_ValueEnumerator5* c_MapValues5::p_ObjectEnumerator(){
	return (new c_ValueEnumerator5)->m_new(m_map->p_FirstNode());
}
void c_MapValues5::mark(){
	Object::mark();
	gc_mark_q(m_map);
}
c_NodeEnumerator5::c_NodeEnumerator5(){
	m_node=0;
}
c_NodeEnumerator5* c_NodeEnumerator5::m_new(c_Node5* t_node){
	gc_assign(this->m_node,t_node);
	return this;
}
c_NodeEnumerator5* c_NodeEnumerator5::m_new2(){
	return this;
}
bool c_NodeEnumerator5::p_HasNext(){
	return m_node!=0;
}
c_Node5* c_NodeEnumerator5::p_NextObject(){
	c_Node5* t_t=m_node;
	gc_assign(m_node,m_node->p_NextNode());
	return t_t;
}
void c_NodeEnumerator5::mark(){
	Object::mark();
	gc_mark_q(m_node);
}
c_KeyEnumerator2::c_KeyEnumerator2(){
	m_node=0;
}
c_KeyEnumerator2* c_KeyEnumerator2::m_new(c_Node6* t_node){
	gc_assign(this->m_node,t_node);
	return this;
}
c_KeyEnumerator2* c_KeyEnumerator2::m_new2(){
	return this;
}
bool c_KeyEnumerator2::p_HasNext(){
	return m_node!=0;
}
int c_KeyEnumerator2::p_NextObject(){
	c_Node6* t_t=m_node;
	gc_assign(m_node,m_node->p_NextNode());
	return t_t->m_key;
}
void c_KeyEnumerator2::mark(){
	Object::mark();
	gc_mark_q(m_node);
}
c_KeyEnumerator3::c_KeyEnumerator3(){
	m_node=0;
}
c_KeyEnumerator3* c_KeyEnumerator3::m_new(c_Node7* t_node){
	gc_assign(this->m_node,t_node);
	return this;
}
c_KeyEnumerator3* c_KeyEnumerator3::m_new2(){
	return this;
}
bool c_KeyEnumerator3::p_HasNext(){
	return m_node!=0;
}
Float c_KeyEnumerator3::p_NextObject(){
	c_Node7* t_t=m_node;
	gc_assign(m_node,m_node->p_NextNode());
	return t_t->m_key;
}
void c_KeyEnumerator3::mark(){
	Object::mark();
	gc_mark_q(m_node);
}
c_KeyEnumerator4::c_KeyEnumerator4(){
	m_node=0;
}
c_KeyEnumerator4* c_KeyEnumerator4::m_new(c_Node8* t_node){
	gc_assign(this->m_node,t_node);
	return this;
}
c_KeyEnumerator4* c_KeyEnumerator4::m_new2(){
	return this;
}
bool c_KeyEnumerator4::p_HasNext(){
	return m_node!=0;
}
String c_KeyEnumerator4::p_NextObject(){
	c_Node8* t_t=m_node;
	gc_assign(m_node,m_node->p_NextNode());
	return t_t->m_key;
}
void c_KeyEnumerator4::mark(){
	Object::mark();
	gc_mark_q(m_node);
}
c_KeyEnumerator5::c_KeyEnumerator5(){
	m_node=0;
}
c_KeyEnumerator5* c_KeyEnumerator5::m_new(c_Node5* t_node){
	gc_assign(this->m_node,t_node);
	return this;
}
c_KeyEnumerator5* c_KeyEnumerator5::m_new2(){
	return this;
}
bool c_KeyEnumerator5::p_HasNext(){
	return m_node!=0;
}
String c_KeyEnumerator5::p_NextObject(){
	c_Node5* t_t=m_node;
	gc_assign(m_node,m_node->p_NextNode());
	return t_t->m_key;
}
void c_KeyEnumerator5::mark(){
	Object::mark();
	gc_mark_q(m_node);
}
c_ValueEnumerator::c_ValueEnumerator(){
	m_node=0;
}
c_ValueEnumerator* c_ValueEnumerator::m_new(c_Node6* t_node){
	gc_assign(this->m_node,t_node);
	return this;
}
c_ValueEnumerator* c_ValueEnumerator::m_new2(){
	return this;
}
bool c_ValueEnumerator::p_HasNext(){
	return m_node!=0;
}
Object* c_ValueEnumerator::p_NextObject(){
	c_Node6* t_t=m_node;
	gc_assign(m_node,m_node->p_NextNode());
	return t_t->m_value;
}
void c_ValueEnumerator::mark(){
	Object::mark();
	gc_mark_q(m_node);
}
c_ValueEnumerator2::c_ValueEnumerator2(){
	m_node=0;
}
c_ValueEnumerator2* c_ValueEnumerator2::m_new(c_Node7* t_node){
	gc_assign(this->m_node,t_node);
	return this;
}
c_ValueEnumerator2* c_ValueEnumerator2::m_new2(){
	return this;
}
bool c_ValueEnumerator2::p_HasNext(){
	return m_node!=0;
}
Object* c_ValueEnumerator2::p_NextObject(){
	c_Node7* t_t=m_node;
	gc_assign(m_node,m_node->p_NextNode());
	return t_t->m_value;
}
void c_ValueEnumerator2::mark(){
	Object::mark();
	gc_mark_q(m_node);
}
c_ValueEnumerator3::c_ValueEnumerator3(){
	m_node=0;
}
c_ValueEnumerator3* c_ValueEnumerator3::m_new(c_Node8* t_node){
	gc_assign(this->m_node,t_node);
	return this;
}
c_ValueEnumerator3* c_ValueEnumerator3::m_new2(){
	return this;
}
bool c_ValueEnumerator3::p_HasNext(){
	return m_node!=0;
}
Object* c_ValueEnumerator3::p_NextObject(){
	c_Node8* t_t=m_node;
	gc_assign(m_node,m_node->p_NextNode());
	return t_t->m_value;
}
void c_ValueEnumerator3::mark(){
	Object::mark();
	gc_mark_q(m_node);
}
c_ValueEnumerator4::c_ValueEnumerator4(){
	m_node=0;
}
c_ValueEnumerator4* c_ValueEnumerator4::m_new(c_Node4* t_node){
	gc_assign(this->m_node,t_node);
	return this;
}
c_ValueEnumerator4* c_ValueEnumerator4::m_new2(){
	return this;
}
bool c_ValueEnumerator4::p_HasNext(){
	return m_node!=0;
}
String c_ValueEnumerator4::p_NextObject(){
	c_Node4* t_t=m_node;
	gc_assign(m_node,m_node->p_NextNode());
	return t_t->m_value;
}
void c_ValueEnumerator4::mark(){
	Object::mark();
	gc_mark_q(m_node);
}
c_ValueEnumerator5::c_ValueEnumerator5(){
	m_node=0;
}
c_ValueEnumerator5* c_ValueEnumerator5::m_new(c_Node5* t_node){
	gc_assign(this->m_node,t_node);
	return this;
}
c_ValueEnumerator5* c_ValueEnumerator5::m_new2(){
	return this;
}
bool c_ValueEnumerator5::p_HasNext(){
	return m_node!=0;
}
c_Image* c_ValueEnumerator5::p_NextObject(){
	c_Node5* t_t=m_node;
	gc_assign(m_node,m_node->p_NextNode());
	return t_t->m_value;
}
void c_ValueEnumerator5::mark(){
	Object::mark();
	gc_mark_q(m_node);
}
c_Enumerator7::c_Enumerator7(){
	m_stack=0;
	m_index=0;
}
c_Enumerator7* c_Enumerator7::m_new(c_Stack* t_stack){
	gc_assign(this->m_stack,t_stack);
	return this;
}
c_Enumerator7* c_Enumerator7::m_new2(){
	return this;
}
bool c_Enumerator7::p_HasNext(){
	return m_index<m_stack->p_Length();
}
int c_Enumerator7::p_NextObject(){
	m_index+=1;
	return m_stack->m_data[m_index-1];
}
void c_Enumerator7::mark(){
	Object::mark();
	gc_mark_q(m_stack);
}
c_BackwardsStack::c_BackwardsStack(){
	m_stack=0;
}
c_BackwardsStack* c_BackwardsStack::m_new(c_Stack* t_stack){
	gc_assign(this->m_stack,t_stack);
	return this;
}
c_BackwardsStack* c_BackwardsStack::m_new2(){
	return this;
}
c_BackwardsEnumerator4* c_BackwardsStack::p_ObjectEnumerator(){
	return (new c_BackwardsEnumerator4)->m_new(m_stack);
}
void c_BackwardsStack::mark(){
	Object::mark();
	gc_mark_q(m_stack);
}
c_Enumerator8::c_Enumerator8(){
	m_stack=0;
	m_index=0;
}
c_Enumerator8* c_Enumerator8::m_new(c_Stack2* t_stack){
	gc_assign(this->m_stack,t_stack);
	return this;
}
c_Enumerator8* c_Enumerator8::m_new2(){
	return this;
}
bool c_Enumerator8::p_HasNext(){
	return m_index<m_stack->p_Length();
}
Float c_Enumerator8::p_NextObject(){
	m_index+=1;
	return m_stack->m_data[m_index-1];
}
void c_Enumerator8::mark(){
	Object::mark();
	gc_mark_q(m_stack);
}
c_BackwardsStack2::c_BackwardsStack2(){
	m_stack=0;
}
c_BackwardsStack2* c_BackwardsStack2::m_new(c_Stack2* t_stack){
	gc_assign(this->m_stack,t_stack);
	return this;
}
c_BackwardsStack2* c_BackwardsStack2::m_new2(){
	return this;
}
c_BackwardsEnumerator5* c_BackwardsStack2::p_ObjectEnumerator(){
	return (new c_BackwardsEnumerator5)->m_new(m_stack);
}
void c_BackwardsStack2::mark(){
	Object::mark();
	gc_mark_q(m_stack);
}
c_Enumerator9::c_Enumerator9(){
	m_stack=0;
	m_index=0;
}
c_Enumerator9* c_Enumerator9::m_new(c_Stack3* t_stack){
	gc_assign(this->m_stack,t_stack);
	return this;
}
c_Enumerator9* c_Enumerator9::m_new2(){
	return this;
}
bool c_Enumerator9::p_HasNext(){
	return m_index<m_stack->p_Length();
}
String c_Enumerator9::p_NextObject(){
	m_index+=1;
	return m_stack->m_data[m_index-1];
}
void c_Enumerator9::mark(){
	Object::mark();
	gc_mark_q(m_stack);
}
c_BackwardsStack3::c_BackwardsStack3(){
	m_stack=0;
}
c_BackwardsStack3* c_BackwardsStack3::m_new(c_Stack3* t_stack){
	gc_assign(this->m_stack,t_stack);
	return this;
}
c_BackwardsStack3* c_BackwardsStack3::m_new2(){
	return this;
}
c_BackwardsEnumerator6* c_BackwardsStack3::p_ObjectEnumerator(){
	return (new c_BackwardsEnumerator6)->m_new(m_stack);
}
void c_BackwardsStack3::mark(){
	Object::mark();
	gc_mark_q(m_stack);
}
c_BackwardsEnumerator4::c_BackwardsEnumerator4(){
	m_stack=0;
	m_index=0;
}
c_BackwardsEnumerator4* c_BackwardsEnumerator4::m_new(c_Stack* t_stack){
	gc_assign(this->m_stack,t_stack);
	m_index=t_stack->m_length;
	return this;
}
c_BackwardsEnumerator4* c_BackwardsEnumerator4::m_new2(){
	return this;
}
bool c_BackwardsEnumerator4::p_HasNext(){
	return m_index>0;
}
int c_BackwardsEnumerator4::p_NextObject(){
	m_index-=1;
	return m_stack->m_data[m_index];
}
void c_BackwardsEnumerator4::mark(){
	Object::mark();
	gc_mark_q(m_stack);
}
c_BackwardsEnumerator5::c_BackwardsEnumerator5(){
	m_stack=0;
	m_index=0;
}
c_BackwardsEnumerator5* c_BackwardsEnumerator5::m_new(c_Stack2* t_stack){
	gc_assign(this->m_stack,t_stack);
	m_index=t_stack->m_length;
	return this;
}
c_BackwardsEnumerator5* c_BackwardsEnumerator5::m_new2(){
	return this;
}
bool c_BackwardsEnumerator5::p_HasNext(){
	return m_index>0;
}
Float c_BackwardsEnumerator5::p_NextObject(){
	m_index-=1;
	return m_stack->m_data[m_index];
}
void c_BackwardsEnumerator5::mark(){
	Object::mark();
	gc_mark_q(m_stack);
}
c_BackwardsEnumerator6::c_BackwardsEnumerator6(){
	m_stack=0;
	m_index=0;
}
c_BackwardsEnumerator6* c_BackwardsEnumerator6::m_new(c_Stack3* t_stack){
	gc_assign(this->m_stack,t_stack);
	m_index=t_stack->m_length;
	return this;
}
c_BackwardsEnumerator6* c_BackwardsEnumerator6::m_new2(){
	return this;
}
bool c_BackwardsEnumerator6::p_HasNext(){
	return m_index>0;
}
String c_BackwardsEnumerator6::p_NextObject(){
	m_index-=1;
	return m_stack->m_data[m_index];
}
void c_BackwardsEnumerator6::mark(){
	Object::mark();
	gc_mark_q(m_stack);
}
c_ArrayObject::c_ArrayObject(){
	m_value=Array<int >();
}
c_ArrayObject* c_ArrayObject::m_new(Array<int > t_value){
	gc_assign(this->m_value,t_value);
	return this;
}
Array<int > c_ArrayObject::p_ToArray(){
	return m_value;
}
c_ArrayObject* c_ArrayObject::m_new2(){
	return this;
}
void c_ArrayObject::mark(){
	Object::mark();
	gc_mark_q(m_value);
}
c_ArrayObject2::c_ArrayObject2(){
	m_value=Array<Float >();
}
c_ArrayObject2* c_ArrayObject2::m_new(Array<Float > t_value){
	gc_assign(this->m_value,t_value);
	return this;
}
Array<Float > c_ArrayObject2::p_ToArray(){
	return m_value;
}
c_ArrayObject2* c_ArrayObject2::m_new2(){
	return this;
}
void c_ArrayObject2::mark(){
	Object::mark();
	gc_mark_q(m_value);
}
c_ArrayObject3::c_ArrayObject3(){
	m_value=Array<String >();
}
c_ArrayObject3* c_ArrayObject3::m_new(Array<String > t_value){
	gc_assign(this->m_value,t_value);
	return this;
}
Array<String > c_ArrayObject3::p_ToArray(){
	return m_value;
}
c_ArrayObject3* c_ArrayObject3::m_new2(){
	return this;
}
void c_ArrayObject3::mark(){
	Object::mark();
	gc_mark_q(m_value);
}
c_ClassInfo::c_ClassInfo(){
	m__name=String();
	m__attrs=0;
	m__sclass=0;
	m__ifaces=Array<c_ClassInfo* >();
	m__rconsts=Array<c_ConstInfo* >();
	m__consts=Array<c_ConstInfo* >();
	m__rfields=Array<c_FieldInfo* >();
	m__fields=Array<c_FieldInfo* >();
	m__rglobals=Array<c_GlobalInfo* >();
	m__globals=Array<c_GlobalInfo* >();
	m__rmethods=Array<c_MethodInfo* >();
	m__methods=Array<c_MethodInfo* >();
	m__rfunctions=Array<c_FunctionInfo* >();
	m__functions=Array<c_FunctionInfo* >();
	m__ctors=Array<c_FunctionInfo* >();
}
c_ClassInfo* c_ClassInfo::m_new(String t_name,int t_attrs,c_ClassInfo* t_sclass,Array<c_ClassInfo* > t_ifaces){
	m__name=t_name;
	m__attrs=t_attrs;
	gc_assign(m__sclass,t_sclass);
	gc_assign(m__ifaces,t_ifaces);
	return this;
}
c_ClassInfo* c_ClassInfo::m_new2(){
	return this;
}
int c_ClassInfo::p_Init3(){
	return 0;
}
int c_ClassInfo::p_InitR(){
	if((m__sclass)!=0){
		c_Stack4* t_consts=(new c_Stack4)->m_new2(m__sclass->m__rconsts);
		Array<c_ConstInfo* > t_=m__consts;
		int t_2=0;
		while(t_2<t_.Length()){
			c_ConstInfo* t_t=t_[t_2];
			t_2=t_2+1;
			t_consts->p_Push10(t_t);
		}
		gc_assign(m__rconsts,t_consts->p_ToArray());
		c_Stack5* t_fields=(new c_Stack5)->m_new2(m__sclass->m__rfields);
		Array<c_FieldInfo* > t_3=m__fields;
		int t_4=0;
		while(t_4<t_3.Length()){
			c_FieldInfo* t_t2=t_3[t_4];
			t_4=t_4+1;
			t_fields->p_Push13(t_t2);
		}
		gc_assign(m__rfields,t_fields->p_ToArray());
		c_Stack6* t_globals=(new c_Stack6)->m_new2(m__sclass->m__rglobals);
		Array<c_GlobalInfo* > t_5=m__globals;
		int t_6=0;
		while(t_6<t_5.Length()){
			c_GlobalInfo* t_t3=t_5[t_6];
			t_6=t_6+1;
			t_globals->p_Push16(t_t3);
		}
		gc_assign(m__rglobals,t_globals->p_ToArray());
		c_Stack7* t_methods=(new c_Stack7)->m_new2(m__sclass->m__rmethods);
		Array<c_MethodInfo* > t_7=m__methods;
		int t_8=0;
		while(t_8<t_7.Length()){
			c_MethodInfo* t_t4=t_7[t_8];
			t_8=t_8+1;
			t_methods->p_Push19(t_t4);
		}
		gc_assign(m__rmethods,t_methods->p_ToArray());
		c_Stack8* t_functions=(new c_Stack8)->m_new2(m__sclass->m__rfunctions);
		Array<c_FunctionInfo* > t_9=m__functions;
		int t_10=0;
		while(t_10<t_9.Length()){
			c_FunctionInfo* t_t5=t_9[t_10];
			t_10=t_10+1;
			t_functions->p_Push22(t_t5);
		}
		gc_assign(m__rfunctions,t_functions->p_ToArray());
	}else{
		gc_assign(m__rconsts,m__consts);
		gc_assign(m__rfields,m__fields);
		gc_assign(m__rglobals,m__globals);
		gc_assign(m__rmethods,m__methods);
		gc_assign(m__rfunctions,m__functions);
	}
	return 0;
}
void c_ClassInfo::mark(){
	Object::mark();
	gc_mark_q(m__sclass);
	gc_mark_q(m__ifaces);
	gc_mark_q(m__rconsts);
	gc_mark_q(m__consts);
	gc_mark_q(m__rfields);
	gc_mark_q(m__fields);
	gc_mark_q(m__rglobals);
	gc_mark_q(m__globals);
	gc_mark_q(m__rmethods);
	gc_mark_q(m__methods);
	gc_mark_q(m__rfunctions);
	gc_mark_q(m__functions);
	gc_mark_q(m__ctors);
}
Array<c_ClassInfo* > bb_reflection__classes;
c_R63::c_R63(){
}
c_R63* c_R63::m_new(){
	c_ClassInfo::m_new(String(L"monkey.lang.Object",18),1,0,Array<c_ClassInfo* >());
	return this;
}
int c_R63::p_Init3(){
	p_InitR();
	return 0;
}
void c_R63::mark(){
	c_ClassInfo::mark();
}
c_R64::c_R64(){
}
c_R64* c_R64::m_new(){
	c_ClassInfo::m_new(String(L"monkey.boxes.BoolObject",23),0,bb_reflection__classes[0],Array<c_ClassInfo* >());
	gc_assign(bb_reflection__boolClass,(this));
	return this;
}
int c_R64::p_Init3(){
	gc_assign(m__fields,Array<c_FieldInfo* >(1));
	gc_assign(m__fields[0],((new c_R65)->m_new()));
	gc_assign(m__methods,Array<c_MethodInfo* >(2));
	gc_assign(m__methods[0],((new c_R67)->m_new()));
	gc_assign(m__methods[1],((new c_R68)->m_new()));
	gc_assign(m__ctors,Array<c_FunctionInfo* >(2));
	gc_assign(m__ctors[0],((new c_R66)->m_new()));
	gc_assign(m__ctors[1],((new c_R69)->m_new()));
	p_InitR();
	return 0;
}
void c_R64::mark(){
	c_ClassInfo::mark();
}
c_ClassInfo* bb_reflection__boolClass;
c_R70::c_R70(){
}
c_R70* c_R70::m_new(){
	c_ClassInfo::m_new(String(L"monkey.boxes.IntObject",22),0,bb_reflection__classes[0],Array<c_ClassInfo* >());
	gc_assign(bb_reflection__intClass,(this));
	return this;
}
int c_R70::p_Init3(){
	gc_assign(m__fields,Array<c_FieldInfo* >(1));
	gc_assign(m__fields[0],((new c_R71)->m_new()));
	gc_assign(m__methods,Array<c_MethodInfo* >(5));
	gc_assign(m__methods[0],((new c_R74)->m_new()));
	gc_assign(m__methods[1],((new c_R75)->m_new()));
	gc_assign(m__methods[2],((new c_R76)->m_new()));
	gc_assign(m__methods[3],((new c_R77)->m_new()));
	gc_assign(m__methods[4],((new c_R78)->m_new()));
	gc_assign(m__ctors,Array<c_FunctionInfo* >(3));
	gc_assign(m__ctors[0],((new c_R72)->m_new()));
	gc_assign(m__ctors[1],((new c_R73)->m_new()));
	gc_assign(m__ctors[2],((new c_R79)->m_new()));
	p_InitR();
	return 0;
}
void c_R70::mark(){
	c_ClassInfo::mark();
}
c_ClassInfo* bb_reflection__intClass;
c_R80::c_R80(){
}
c_R80* c_R80::m_new(){
	c_ClassInfo::m_new(String(L"monkey.boxes.FloatObject",24),0,bb_reflection__classes[0],Array<c_ClassInfo* >());
	gc_assign(bb_reflection__floatClass,(this));
	return this;
}
int c_R80::p_Init3(){
	gc_assign(m__fields,Array<c_FieldInfo* >(1));
	gc_assign(m__fields[0],((new c_R81)->m_new()));
	gc_assign(m__methods,Array<c_MethodInfo* >(5));
	gc_assign(m__methods[0],((new c_R84)->m_new()));
	gc_assign(m__methods[1],((new c_R85)->m_new()));
	gc_assign(m__methods[2],((new c_R86)->m_new()));
	gc_assign(m__methods[3],((new c_R87)->m_new()));
	gc_assign(m__methods[4],((new c_R88)->m_new()));
	gc_assign(m__ctors,Array<c_FunctionInfo* >(3));
	gc_assign(m__ctors[0],((new c_R82)->m_new()));
	gc_assign(m__ctors[1],((new c_R83)->m_new()));
	gc_assign(m__ctors[2],((new c_R89)->m_new()));
	p_InitR();
	return 0;
}
void c_R80::mark(){
	c_ClassInfo::mark();
}
c_ClassInfo* bb_reflection__floatClass;
c_R90::c_R90(){
}
c_R90* c_R90::m_new(){
	c_ClassInfo::m_new(String(L"monkey.boxes.StringObject",25),0,bb_reflection__classes[0],Array<c_ClassInfo* >());
	gc_assign(bb_reflection__stringClass,(this));
	return this;
}
int c_R90::p_Init3(){
	gc_assign(m__fields,Array<c_FieldInfo* >(1));
	gc_assign(m__fields[0],((new c_R91)->m_new()));
	gc_assign(m__methods,Array<c_MethodInfo* >(3));
	gc_assign(m__methods[0],((new c_R95)->m_new()));
	gc_assign(m__methods[1],((new c_R96)->m_new()));
	gc_assign(m__methods[2],((new c_R97)->m_new()));
	gc_assign(m__ctors,Array<c_FunctionInfo* >(4));
	gc_assign(m__ctors[0],((new c_R92)->m_new()));
	gc_assign(m__ctors[1],((new c_R93)->m_new()));
	gc_assign(m__ctors[2],((new c_R94)->m_new()));
	gc_assign(m__ctors[3],((new c_R98)->m_new()));
	p_InitR();
	return 0;
}
void c_R90::mark(){
	c_ClassInfo::mark();
}
c_ClassInfo* bb_reflection__stringClass;
c_R99::c_R99(){
}
c_R99* c_R99::m_new(){
	c_ClassInfo::m_new(String(L"monkey.deque.Deque<Int>",23),0,bb_reflection__classes[0],Array<c_ClassInfo* >());
	return this;
}
int c_R99::p_Init3(){
	gc_assign(m__globals,Array<c_GlobalInfo* >(1));
	gc_assign(m__globals[0],((new c_R115)->m_new()));
	gc_assign(m__fields,Array<c_FieldInfo* >(4));
	gc_assign(m__fields[0],((new c_R116)->m_new()));
	gc_assign(m__fields[1],((new c_R117)->m_new()));
	gc_assign(m__fields[2],((new c_R118)->m_new()));
	gc_assign(m__fields[3],((new c_R119)->m_new()));
	gc_assign(m__methods,Array<c_MethodInfo* >(14));
	gc_assign(m__methods[0],((new c_R102)->m_new()));
	gc_assign(m__methods[1],((new c_R103)->m_new()));
	gc_assign(m__methods[2],((new c_R104)->m_new()));
	gc_assign(m__methods[3],((new c_R105)->m_new()));
	gc_assign(m__methods[4],((new c_R106)->m_new()));
	gc_assign(m__methods[5],((new c_R107)->m_new()));
	gc_assign(m__methods[6],((new c_R108)->m_new()));
	gc_assign(m__methods[7],((new c_R109)->m_new()));
	gc_assign(m__methods[8],((new c_R110)->m_new()));
	gc_assign(m__methods[9],((new c_R111)->m_new()));
	gc_assign(m__methods[10],((new c_R112)->m_new()));
	gc_assign(m__methods[11],((new c_R113)->m_new()));
	gc_assign(m__methods[12],((new c_R114)->m_new()));
	gc_assign(m__methods[13],((new c_R120)->m_new()));
	gc_assign(m__ctors,Array<c_FunctionInfo* >(2));
	gc_assign(m__ctors[0],((new c_R100)->m_new()));
	gc_assign(m__ctors[1],((new c_R101)->m_new()));
	p_InitR();
	return 0;
}
void c_R99::mark(){
	c_ClassInfo::mark();
}
c_R121::c_R121(){
}
c_R121* c_R121::m_new(){
	c_ClassInfo::m_new(String(L"monkey.deque.IntDeque",21),0,bb_reflection__classes[5],Array<c_ClassInfo* >());
	return this;
}
int c_R121::p_Init3(){
	gc_assign(m__ctors,Array<c_FunctionInfo* >(2));
	gc_assign(m__ctors[0],((new c_R122)->m_new()));
	gc_assign(m__ctors[1],((new c_R123)->m_new()));
	p_InitR();
	return 0;
}
void c_R121::mark(){
	c_ClassInfo::mark();
}
c_R124::c_R124(){
}
c_R124* c_R124::m_new(){
	c_ClassInfo::m_new(String(L"monkey.deque.Deque<Float>",25),0,bb_reflection__classes[0],Array<c_ClassInfo* >());
	return this;
}
int c_R124::p_Init3(){
	gc_assign(m__globals,Array<c_GlobalInfo* >(1));
	gc_assign(m__globals[0],((new c_R140)->m_new()));
	gc_assign(m__fields,Array<c_FieldInfo* >(4));
	gc_assign(m__fields[0],((new c_R141)->m_new()));
	gc_assign(m__fields[1],((new c_R142)->m_new()));
	gc_assign(m__fields[2],((new c_R143)->m_new()));
	gc_assign(m__fields[3],((new c_R144)->m_new()));
	gc_assign(m__methods,Array<c_MethodInfo* >(14));
	gc_assign(m__methods[0],((new c_R127)->m_new()));
	gc_assign(m__methods[1],((new c_R128)->m_new()));
	gc_assign(m__methods[2],((new c_R129)->m_new()));
	gc_assign(m__methods[3],((new c_R130)->m_new()));
	gc_assign(m__methods[4],((new c_R131)->m_new()));
	gc_assign(m__methods[5],((new c_R132)->m_new()));
	gc_assign(m__methods[6],((new c_R133)->m_new()));
	gc_assign(m__methods[7],((new c_R134)->m_new()));
	gc_assign(m__methods[8],((new c_R135)->m_new()));
	gc_assign(m__methods[9],((new c_R136)->m_new()));
	gc_assign(m__methods[10],((new c_R137)->m_new()));
	gc_assign(m__methods[11],((new c_R138)->m_new()));
	gc_assign(m__methods[12],((new c_R139)->m_new()));
	gc_assign(m__methods[13],((new c_R145)->m_new()));
	gc_assign(m__ctors,Array<c_FunctionInfo* >(2));
	gc_assign(m__ctors[0],((new c_R125)->m_new()));
	gc_assign(m__ctors[1],((new c_R126)->m_new()));
	p_InitR();
	return 0;
}
void c_R124::mark(){
	c_ClassInfo::mark();
}
c_R146::c_R146(){
}
c_R146* c_R146::m_new(){
	c_ClassInfo::m_new(String(L"monkey.deque.FloatDeque",23),0,bb_reflection__classes[7],Array<c_ClassInfo* >());
	return this;
}
int c_R146::p_Init3(){
	gc_assign(m__ctors,Array<c_FunctionInfo* >(2));
	gc_assign(m__ctors[0],((new c_R147)->m_new()));
	gc_assign(m__ctors[1],((new c_R148)->m_new()));
	p_InitR();
	return 0;
}
void c_R146::mark(){
	c_ClassInfo::mark();
}
c_R149::c_R149(){
}
c_R149* c_R149::m_new(){
	c_ClassInfo::m_new(String(L"monkey.deque.Deque<String>",26),0,bb_reflection__classes[0],Array<c_ClassInfo* >());
	return this;
}
int c_R149::p_Init3(){
	gc_assign(m__globals,Array<c_GlobalInfo* >(1));
	gc_assign(m__globals[0],((new c_R165)->m_new()));
	gc_assign(m__fields,Array<c_FieldInfo* >(4));
	gc_assign(m__fields[0],((new c_R166)->m_new()));
	gc_assign(m__fields[1],((new c_R167)->m_new()));
	gc_assign(m__fields[2],((new c_R168)->m_new()));
	gc_assign(m__fields[3],((new c_R169)->m_new()));
	gc_assign(m__methods,Array<c_MethodInfo* >(14));
	gc_assign(m__methods[0],((new c_R152)->m_new()));
	gc_assign(m__methods[1],((new c_R153)->m_new()));
	gc_assign(m__methods[2],((new c_R154)->m_new()));
	gc_assign(m__methods[3],((new c_R155)->m_new()));
	gc_assign(m__methods[4],((new c_R156)->m_new()));
	gc_assign(m__methods[5],((new c_R157)->m_new()));
	gc_assign(m__methods[6],((new c_R158)->m_new()));
	gc_assign(m__methods[7],((new c_R159)->m_new()));
	gc_assign(m__methods[8],((new c_R160)->m_new()));
	gc_assign(m__methods[9],((new c_R161)->m_new()));
	gc_assign(m__methods[10],((new c_R162)->m_new()));
	gc_assign(m__methods[11],((new c_R163)->m_new()));
	gc_assign(m__methods[12],((new c_R164)->m_new()));
	gc_assign(m__methods[13],((new c_R170)->m_new()));
	gc_assign(m__ctors,Array<c_FunctionInfo* >(2));
	gc_assign(m__ctors[0],((new c_R150)->m_new()));
	gc_assign(m__ctors[1],((new c_R151)->m_new()));
	p_InitR();
	return 0;
}
void c_R149::mark(){
	c_ClassInfo::mark();
}
c_R171::c_R171(){
}
c_R171* c_R171::m_new(){
	c_ClassInfo::m_new(String(L"monkey.deque.StringDeque",24),0,bb_reflection__classes[9],Array<c_ClassInfo* >());
	return this;
}
int c_R171::p_Init3(){
	gc_assign(m__ctors,Array<c_FunctionInfo* >(2));
	gc_assign(m__ctors[0],((new c_R172)->m_new()));
	gc_assign(m__ctors[1],((new c_R173)->m_new()));
	p_InitR();
	return 0;
}
void c_R171::mark(){
	c_ClassInfo::mark();
}
c_R174::c_R174(){
}
c_R174* c_R174::m_new(){
	c_ClassInfo::m_new(String(L"monkey.lang.Throwable",21),33,bb_reflection__classes[0],Array<c_ClassInfo* >());
	return this;
}
int c_R174::p_Init3(){
	p_InitR();
	return 0;
}
void c_R174::mark(){
	c_ClassInfo::mark();
}
c_R175::c_R175(){
}
c_R175* c_R175::m_new(){
	c_ClassInfo::m_new(String(L"monkey.list.List<Int>",21),0,bb_reflection__classes[0],Array<c_ClassInfo* >());
	return this;
}
int c_R175::p_Init3(){
	gc_assign(m__fields,Array<c_FieldInfo* >(1));
	gc_assign(m__fields[0],((new c_R208)->m_new()));
	gc_assign(m__methods,Array<c_MethodInfo* >(30));
	gc_assign(m__methods[0],((new c_R178)->m_new()));
	gc_assign(m__methods[1],((new c_R179)->m_new()));
	gc_assign(m__methods[2],((new c_R180)->m_new()));
	gc_assign(m__methods[3],((new c_R181)->m_new()));
	gc_assign(m__methods[4],((new c_R182)->m_new()));
	gc_assign(m__methods[5],((new c_R183)->m_new()));
	gc_assign(m__methods[6],((new c_R184)->m_new()));
	gc_assign(m__methods[7],((new c_R185)->m_new()));
	gc_assign(m__methods[8],((new c_R186)->m_new()));
	gc_assign(m__methods[9],((new c_R187)->m_new()));
	gc_assign(m__methods[10],((new c_R188)->m_new()));
	gc_assign(m__methods[11],((new c_R189)->m_new()));
	gc_assign(m__methods[12],((new c_R190)->m_new()));
	gc_assign(m__methods[13],((new c_R191)->m_new()));
	gc_assign(m__methods[14],((new c_R192)->m_new()));
	gc_assign(m__methods[15],((new c_R193)->m_new()));
	gc_assign(m__methods[16],((new c_R194)->m_new()));
	gc_assign(m__methods[17],((new c_R195)->m_new()));
	gc_assign(m__methods[18],((new c_R196)->m_new()));
	gc_assign(m__methods[19],((new c_R197)->m_new()));
	gc_assign(m__methods[20],((new c_R198)->m_new()));
	gc_assign(m__methods[21],((new c_R199)->m_new()));
	gc_assign(m__methods[22],((new c_R200)->m_new()));
	gc_assign(m__methods[23],((new c_R201)->m_new()));
	gc_assign(m__methods[24],((new c_R202)->m_new()));
	gc_assign(m__methods[25],((new c_R203)->m_new()));
	gc_assign(m__methods[26],((new c_R204)->m_new()));
	gc_assign(m__methods[27],((new c_R205)->m_new()));
	gc_assign(m__methods[28],((new c_R206)->m_new()));
	gc_assign(m__methods[29],((new c_R207)->m_new()));
	gc_assign(m__ctors,Array<c_FunctionInfo* >(2));
	gc_assign(m__ctors[0],((new c_R176)->m_new()));
	gc_assign(m__ctors[1],((new c_R177)->m_new()));
	p_InitR();
	return 0;
}
void c_R175::mark(){
	c_ClassInfo::mark();
}
c_R209::c_R209(){
}
c_R209* c_R209::m_new(){
	c_ClassInfo::m_new(String(L"monkey.list.IntList",19),0,bb_reflection__classes[12],Array<c_ClassInfo* >());
	return this;
}
int c_R209::p_Init3(){
	gc_assign(m__methods,Array<c_MethodInfo* >(2));
	gc_assign(m__methods[0],((new c_R211)->m_new()));
	gc_assign(m__methods[1],((new c_R212)->m_new()));
	gc_assign(m__ctors,Array<c_FunctionInfo* >(2));
	gc_assign(m__ctors[0],((new c_R210)->m_new()));
	gc_assign(m__ctors[1],((new c_R213)->m_new()));
	p_InitR();
	return 0;
}
void c_R209::mark(){
	c_ClassInfo::mark();
}
c_R214::c_R214(){
}
c_R214* c_R214::m_new(){
	c_ClassInfo::m_new(String(L"monkey.list.Node<Int>",21),0,bb_reflection__classes[0],Array<c_ClassInfo* >());
	return this;
}
int c_R214::p_Init3(){
	gc_assign(m__fields,Array<c_FieldInfo* >(3));
	gc_assign(m__fields[0],((new c_R220)->m_new()));
	gc_assign(m__fields[1],((new c_R221)->m_new()));
	gc_assign(m__fields[2],((new c_R222)->m_new()));
	gc_assign(m__methods,Array<c_MethodInfo* >(5));
	gc_assign(m__methods[0],((new c_R216)->m_new()));
	gc_assign(m__methods[1],((new c_R217)->m_new()));
	gc_assign(m__methods[2],((new c_R218)->m_new()));
	gc_assign(m__methods[3],((new c_R219)->m_new()));
	gc_assign(m__methods[4],((new c_R223)->m_new()));
	gc_assign(m__ctors,Array<c_FunctionInfo* >(2));
	gc_assign(m__ctors[0],((new c_R215)->m_new()));
	gc_assign(m__ctors[1],((new c_R224)->m_new()));
	p_InitR();
	return 0;
}
void c_R214::mark(){
	c_ClassInfo::mark();
}
c_R225::c_R225(){
}
c_R225* c_R225::m_new(){
	c_ClassInfo::m_new(String(L"monkey.list.HeadNode<Int>",25),2,bb_reflection__classes[14],Array<c_ClassInfo* >());
	return this;
}
int c_R225::p_Init3(){
	gc_assign(m__methods,Array<c_MethodInfo* >(1));
	gc_assign(m__methods[0],((new c_R227)->m_new()));
	gc_assign(m__ctors,Array<c_FunctionInfo* >(1));
	gc_assign(m__ctors[0],((new c_R226)->m_new()));
	p_InitR();
	return 0;
}
void c_R225::mark(){
	c_ClassInfo::mark();
}
c_R228::c_R228(){
}
c_R228* c_R228::m_new(){
	c_ClassInfo::m_new(String(L"monkey.list.List<Float>",23),0,bb_reflection__classes[0],Array<c_ClassInfo* >());
	return this;
}
int c_R228::p_Init3(){
	gc_assign(m__fields,Array<c_FieldInfo* >(1));
	gc_assign(m__fields[0],((new c_R261)->m_new()));
	gc_assign(m__methods,Array<c_MethodInfo* >(30));
	gc_assign(m__methods[0],((new c_R231)->m_new()));
	gc_assign(m__methods[1],((new c_R232)->m_new()));
	gc_assign(m__methods[2],((new c_R233)->m_new()));
	gc_assign(m__methods[3],((new c_R234)->m_new()));
	gc_assign(m__methods[4],((new c_R235)->m_new()));
	gc_assign(m__methods[5],((new c_R236)->m_new()));
	gc_assign(m__methods[6],((new c_R237)->m_new()));
	gc_assign(m__methods[7],((new c_R238)->m_new()));
	gc_assign(m__methods[8],((new c_R239)->m_new()));
	gc_assign(m__methods[9],((new c_R240)->m_new()));
	gc_assign(m__methods[10],((new c_R241)->m_new()));
	gc_assign(m__methods[11],((new c_R242)->m_new()));
	gc_assign(m__methods[12],((new c_R243)->m_new()));
	gc_assign(m__methods[13],((new c_R244)->m_new()));
	gc_assign(m__methods[14],((new c_R245)->m_new()));
	gc_assign(m__methods[15],((new c_R246)->m_new()));
	gc_assign(m__methods[16],((new c_R247)->m_new()));
	gc_assign(m__methods[17],((new c_R248)->m_new()));
	gc_assign(m__methods[18],((new c_R249)->m_new()));
	gc_assign(m__methods[19],((new c_R250)->m_new()));
	gc_assign(m__methods[20],((new c_R251)->m_new()));
	gc_assign(m__methods[21],((new c_R252)->m_new()));
	gc_assign(m__methods[22],((new c_R253)->m_new()));
	gc_assign(m__methods[23],((new c_R254)->m_new()));
	gc_assign(m__methods[24],((new c_R255)->m_new()));
	gc_assign(m__methods[25],((new c_R256)->m_new()));
	gc_assign(m__methods[26],((new c_R257)->m_new()));
	gc_assign(m__methods[27],((new c_R258)->m_new()));
	gc_assign(m__methods[28],((new c_R259)->m_new()));
	gc_assign(m__methods[29],((new c_R260)->m_new()));
	gc_assign(m__ctors,Array<c_FunctionInfo* >(2));
	gc_assign(m__ctors[0],((new c_R229)->m_new()));
	gc_assign(m__ctors[1],((new c_R230)->m_new()));
	p_InitR();
	return 0;
}
void c_R228::mark(){
	c_ClassInfo::mark();
}
c_R262::c_R262(){
}
c_R262* c_R262::m_new(){
	c_ClassInfo::m_new(String(L"monkey.list.FloatList",21),0,bb_reflection__classes[16],Array<c_ClassInfo* >());
	return this;
}
int c_R262::p_Init3(){
	gc_assign(m__methods,Array<c_MethodInfo* >(2));
	gc_assign(m__methods[0],((new c_R264)->m_new()));
	gc_assign(m__methods[1],((new c_R265)->m_new()));
	gc_assign(m__ctors,Array<c_FunctionInfo* >(2));
	gc_assign(m__ctors[0],((new c_R263)->m_new()));
	gc_assign(m__ctors[1],((new c_R266)->m_new()));
	p_InitR();
	return 0;
}
void c_R262::mark(){
	c_ClassInfo::mark();
}
c_R267::c_R267(){
}
c_R267* c_R267::m_new(){
	c_ClassInfo::m_new(String(L"monkey.list.Node<Float>",23),0,bb_reflection__classes[0],Array<c_ClassInfo* >());
	return this;
}
int c_R267::p_Init3(){
	gc_assign(m__fields,Array<c_FieldInfo* >(3));
	gc_assign(m__fields[0],((new c_R273)->m_new()));
	gc_assign(m__fields[1],((new c_R274)->m_new()));
	gc_assign(m__fields[2],((new c_R275)->m_new()));
	gc_assign(m__methods,Array<c_MethodInfo* >(5));
	gc_assign(m__methods[0],((new c_R269)->m_new()));
	gc_assign(m__methods[1],((new c_R270)->m_new()));
	gc_assign(m__methods[2],((new c_R271)->m_new()));
	gc_assign(m__methods[3],((new c_R272)->m_new()));
	gc_assign(m__methods[4],((new c_R276)->m_new()));
	gc_assign(m__ctors,Array<c_FunctionInfo* >(2));
	gc_assign(m__ctors[0],((new c_R268)->m_new()));
	gc_assign(m__ctors[1],((new c_R277)->m_new()));
	p_InitR();
	return 0;
}
void c_R267::mark(){
	c_ClassInfo::mark();
}
c_R278::c_R278(){
}
c_R278* c_R278::m_new(){
	c_ClassInfo::m_new(String(L"monkey.list.HeadNode<Float>",27),2,bb_reflection__classes[18],Array<c_ClassInfo* >());
	return this;
}
int c_R278::p_Init3(){
	gc_assign(m__methods,Array<c_MethodInfo* >(1));
	gc_assign(m__methods[0],((new c_R280)->m_new()));
	gc_assign(m__ctors,Array<c_FunctionInfo* >(1));
	gc_assign(m__ctors[0],((new c_R279)->m_new()));
	p_InitR();
	return 0;
}
void c_R278::mark(){
	c_ClassInfo::mark();
}
c_R281::c_R281(){
}
c_R281* c_R281::m_new(){
	c_ClassInfo::m_new(String(L"monkey.list.List<String>",24),0,bb_reflection__classes[0],Array<c_ClassInfo* >());
	return this;
}
int c_R281::p_Init3(){
	gc_assign(m__fields,Array<c_FieldInfo* >(1));
	gc_assign(m__fields[0],((new c_R314)->m_new()));
	gc_assign(m__methods,Array<c_MethodInfo* >(30));
	gc_assign(m__methods[0],((new c_R284)->m_new()));
	gc_assign(m__methods[1],((new c_R285)->m_new()));
	gc_assign(m__methods[2],((new c_R286)->m_new()));
	gc_assign(m__methods[3],((new c_R287)->m_new()));
	gc_assign(m__methods[4],((new c_R288)->m_new()));
	gc_assign(m__methods[5],((new c_R289)->m_new()));
	gc_assign(m__methods[6],((new c_R290)->m_new()));
	gc_assign(m__methods[7],((new c_R291)->m_new()));
	gc_assign(m__methods[8],((new c_R292)->m_new()));
	gc_assign(m__methods[9],((new c_R293)->m_new()));
	gc_assign(m__methods[10],((new c_R294)->m_new()));
	gc_assign(m__methods[11],((new c_R295)->m_new()));
	gc_assign(m__methods[12],((new c_R296)->m_new()));
	gc_assign(m__methods[13],((new c_R297)->m_new()));
	gc_assign(m__methods[14],((new c_R298)->m_new()));
	gc_assign(m__methods[15],((new c_R299)->m_new()));
	gc_assign(m__methods[16],((new c_R300)->m_new()));
	gc_assign(m__methods[17],((new c_R301)->m_new()));
	gc_assign(m__methods[18],((new c_R302)->m_new()));
	gc_assign(m__methods[19],((new c_R303)->m_new()));
	gc_assign(m__methods[20],((new c_R304)->m_new()));
	gc_assign(m__methods[21],((new c_R305)->m_new()));
	gc_assign(m__methods[22],((new c_R306)->m_new()));
	gc_assign(m__methods[23],((new c_R307)->m_new()));
	gc_assign(m__methods[24],((new c_R308)->m_new()));
	gc_assign(m__methods[25],((new c_R309)->m_new()));
	gc_assign(m__methods[26],((new c_R310)->m_new()));
	gc_assign(m__methods[27],((new c_R311)->m_new()));
	gc_assign(m__methods[28],((new c_R312)->m_new()));
	gc_assign(m__methods[29],((new c_R313)->m_new()));
	gc_assign(m__ctors,Array<c_FunctionInfo* >(2));
	gc_assign(m__ctors[0],((new c_R282)->m_new()));
	gc_assign(m__ctors[1],((new c_R283)->m_new()));
	p_InitR();
	return 0;
}
void c_R281::mark(){
	c_ClassInfo::mark();
}
c_R315::c_R315(){
}
c_R315* c_R315::m_new(){
	c_ClassInfo::m_new(String(L"monkey.list.StringList",22),0,bb_reflection__classes[20],Array<c_ClassInfo* >());
	return this;
}
int c_R315::p_Init3(){
	gc_assign(m__methods,Array<c_MethodInfo* >(3));
	gc_assign(m__methods[0],((new c_R317)->m_new()));
	gc_assign(m__methods[1],((new c_R318)->m_new()));
	gc_assign(m__methods[2],((new c_R319)->m_new()));
	gc_assign(m__ctors,Array<c_FunctionInfo* >(2));
	gc_assign(m__ctors[0],((new c_R316)->m_new()));
	gc_assign(m__ctors[1],((new c_R320)->m_new()));
	p_InitR();
	return 0;
}
void c_R315::mark(){
	c_ClassInfo::mark();
}
c_R321::c_R321(){
}
c_R321* c_R321::m_new(){
	c_ClassInfo::m_new(String(L"monkey.list.Node<String>",24),0,bb_reflection__classes[0],Array<c_ClassInfo* >());
	return this;
}
int c_R321::p_Init3(){
	gc_assign(m__fields,Array<c_FieldInfo* >(3));
	gc_assign(m__fields[0],((new c_R327)->m_new()));
	gc_assign(m__fields[1],((new c_R328)->m_new()));
	gc_assign(m__fields[2],((new c_R329)->m_new()));
	gc_assign(m__methods,Array<c_MethodInfo* >(5));
	gc_assign(m__methods[0],((new c_R323)->m_new()));
	gc_assign(m__methods[1],((new c_R324)->m_new()));
	gc_assign(m__methods[2],((new c_R325)->m_new()));
	gc_assign(m__methods[3],((new c_R326)->m_new()));
	gc_assign(m__methods[4],((new c_R330)->m_new()));
	gc_assign(m__ctors,Array<c_FunctionInfo* >(2));
	gc_assign(m__ctors[0],((new c_R322)->m_new()));
	gc_assign(m__ctors[1],((new c_R331)->m_new()));
	p_InitR();
	return 0;
}
void c_R321::mark(){
	c_ClassInfo::mark();
}
c_R332::c_R332(){
}
c_R332* c_R332::m_new(){
	c_ClassInfo::m_new(String(L"monkey.list.HeadNode<String>",28),2,bb_reflection__classes[22],Array<c_ClassInfo* >());
	return this;
}
int c_R332::p_Init3(){
	gc_assign(m__methods,Array<c_MethodInfo* >(1));
	gc_assign(m__methods[0],((new c_R334)->m_new()));
	gc_assign(m__ctors,Array<c_FunctionInfo* >(1));
	gc_assign(m__ctors[0],((new c_R333)->m_new()));
	p_InitR();
	return 0;
}
void c_R332::mark(){
	c_ClassInfo::mark();
}
c_R335::c_R335(){
}
c_R335* c_R335::m_new(){
	c_ClassInfo::m_new(String(L"monkey.list.Enumerator<String>",30),0,bb_reflection__classes[0],Array<c_ClassInfo* >());
	return this;
}
int c_R335::p_Init3(){
	gc_assign(m__fields,Array<c_FieldInfo* >(2));
	gc_assign(m__fields[0],((new c_R339)->m_new()));
	gc_assign(m__fields[1],((new c_R340)->m_new()));
	gc_assign(m__methods,Array<c_MethodInfo* >(2));
	gc_assign(m__methods[0],((new c_R337)->m_new()));
	gc_assign(m__methods[1],((new c_R338)->m_new()));
	gc_assign(m__ctors,Array<c_FunctionInfo* >(2));
	gc_assign(m__ctors[0],((new c_R336)->m_new()));
	gc_assign(m__ctors[1],((new c_R341)->m_new()));
	p_InitR();
	return 0;
}
void c_R335::mark(){
	c_ClassInfo::mark();
}
c_R342::c_R342(){
}
c_R342* c_R342::m_new(){
	c_ClassInfo::m_new(String(L"monkey.set.Set<Int>",19),0,bb_reflection__classes[0],Array<c_ClassInfo* >());
	return this;
}
int c_R342::p_Init3(){
	gc_assign(m__fields,Array<c_FieldInfo* >(1));
	gc_assign(m__fields[0],((new c_R351)->m_new()));
	gc_assign(m__methods,Array<c_MethodInfo* >(7));
	gc_assign(m__methods[0],((new c_R344)->m_new()));
	gc_assign(m__methods[1],((new c_R345)->m_new()));
	gc_assign(m__methods[2],((new c_R346)->m_new()));
	gc_assign(m__methods[3],((new c_R347)->m_new()));
	gc_assign(m__methods[4],((new c_R348)->m_new()));
	gc_assign(m__methods[5],((new c_R349)->m_new()));
	gc_assign(m__methods[6],((new c_R350)->m_new()));
	gc_assign(m__ctors,Array<c_FunctionInfo* >(2));
	gc_assign(m__ctors[0],((new c_R343)->m_new()));
	gc_assign(m__ctors[1],((new c_R352)->m_new()));
	p_InitR();
	return 0;
}
void c_R342::mark(){
	c_ClassInfo::mark();
}
c_R353::c_R353(){
}
c_R353* c_R353::m_new(){
	c_ClassInfo::m_new(String(L"monkey.set.IntSet",17),0,bb_reflection__classes[25],Array<c_ClassInfo* >());
	return this;
}
int c_R353::p_Init3(){
	gc_assign(m__ctors,Array<c_FunctionInfo* >(1));
	gc_assign(m__ctors[0],((new c_R354)->m_new()));
	p_InitR();
	return 0;
}
void c_R353::mark(){
	c_ClassInfo::mark();
}
c_R355::c_R355(){
}
c_R355* c_R355::m_new(){
	c_ClassInfo::m_new(String(L"monkey.map.Map<Int,monkey.lang.Object>",38),4,bb_reflection__classes[0],Array<c_ClassInfo* >());
	return this;
}
int c_R355::p_Init3(){
	gc_assign(m__consts,Array<c_ConstInfo* >(2));
	gc_assign(m__consts[0],(new c_ConstInfo)->m_new(String(L"RED",3),2,bb_reflection__intClass,((new c_IntObject)->m_new(-1))));
	gc_assign(m__consts[1],(new c_ConstInfo)->m_new(String(L"BLACK",5),2,bb_reflection__intClass,((new c_IntObject)->m_new(1))));
	gc_assign(m__fields,Array<c_FieldInfo* >(1));
	gc_assign(m__fields[0],((new c_R379)->m_new()));
	gc_assign(m__methods,Array<c_MethodInfo* >(23));
	gc_assign(m__methods[0],((new c_R356)->m_new()));
	gc_assign(m__methods[1],((new c_R357)->m_new()));
	gc_assign(m__methods[2],((new c_R358)->m_new()));
	gc_assign(m__methods[3],((new c_R359)->m_new()));
	gc_assign(m__methods[4],((new c_R360)->m_new()));
	gc_assign(m__methods[5],((new c_R361)->m_new()));
	gc_assign(m__methods[6],((new c_R362)->m_new()));
	gc_assign(m__methods[7],((new c_R363)->m_new()));
	gc_assign(m__methods[8],((new c_R364)->m_new()));
	gc_assign(m__methods[9],((new c_R365)->m_new()));
	gc_assign(m__methods[10],((new c_R366)->m_new()));
	gc_assign(m__methods[11],((new c_R367)->m_new()));
	gc_assign(m__methods[12],((new c_R368)->m_new()));
	gc_assign(m__methods[13],((new c_R369)->m_new()));
	gc_assign(m__methods[14],((new c_R370)->m_new()));
	gc_assign(m__methods[15],((new c_R371)->m_new()));
	gc_assign(m__methods[16],((new c_R372)->m_new()));
	gc_assign(m__methods[17],((new c_R373)->m_new()));
	gc_assign(m__methods[18],((new c_R374)->m_new()));
	gc_assign(m__methods[19],((new c_R375)->m_new()));
	gc_assign(m__methods[20],((new c_R376)->m_new()));
	gc_assign(m__methods[21],((new c_R377)->m_new()));
	gc_assign(m__methods[22],((new c_R378)->m_new()));
	gc_assign(m__ctors,Array<c_FunctionInfo* >(1));
	gc_assign(m__ctors[0],((new c_R380)->m_new()));
	p_InitR();
	return 0;
}
void c_R355::mark(){
	c_ClassInfo::mark();
}
c_R381::c_R381(){
}
c_R381* c_R381::m_new(){
	c_ClassInfo::m_new(String(L"monkey.map.IntMap<monkey.lang.Object>",37),0,bb_reflection__classes[27],Array<c_ClassInfo* >());
	return this;
}
int c_R381::p_Init3(){
	gc_assign(m__methods,Array<c_MethodInfo* >(1));
	gc_assign(m__methods[0],((new c_R382)->m_new()));
	gc_assign(m__ctors,Array<c_FunctionInfo* >(1));
	gc_assign(m__ctors[0],((new c_R383)->m_new()));
	p_InitR();
	return 0;
}
void c_R381::mark(){
	c_ClassInfo::mark();
}
c_R384::c_R384(){
}
c_R384* c_R384::m_new(){
	c_ClassInfo::m_new(String(L"monkey.set.Set<Float>",21),0,bb_reflection__classes[0],Array<c_ClassInfo* >());
	return this;
}
int c_R384::p_Init3(){
	gc_assign(m__fields,Array<c_FieldInfo* >(1));
	gc_assign(m__fields[0],((new c_R393)->m_new()));
	gc_assign(m__methods,Array<c_MethodInfo* >(7));
	gc_assign(m__methods[0],((new c_R386)->m_new()));
	gc_assign(m__methods[1],((new c_R387)->m_new()));
	gc_assign(m__methods[2],((new c_R388)->m_new()));
	gc_assign(m__methods[3],((new c_R389)->m_new()));
	gc_assign(m__methods[4],((new c_R390)->m_new()));
	gc_assign(m__methods[5],((new c_R391)->m_new()));
	gc_assign(m__methods[6],((new c_R392)->m_new()));
	gc_assign(m__ctors,Array<c_FunctionInfo* >(2));
	gc_assign(m__ctors[0],((new c_R385)->m_new()));
	gc_assign(m__ctors[1],((new c_R394)->m_new()));
	p_InitR();
	return 0;
}
void c_R384::mark(){
	c_ClassInfo::mark();
}
c_R395::c_R395(){
}
c_R395* c_R395::m_new(){
	c_ClassInfo::m_new(String(L"monkey.set.FloatSet",19),0,bb_reflection__classes[29],Array<c_ClassInfo* >());
	return this;
}
int c_R395::p_Init3(){
	gc_assign(m__ctors,Array<c_FunctionInfo* >(1));
	gc_assign(m__ctors[0],((new c_R396)->m_new()));
	p_InitR();
	return 0;
}
void c_R395::mark(){
	c_ClassInfo::mark();
}
c_R397::c_R397(){
}
c_R397* c_R397::m_new(){
	c_ClassInfo::m_new(String(L"monkey.map.Map<Float,monkey.lang.Object>",40),4,bb_reflection__classes[0],Array<c_ClassInfo* >());
	return this;
}
int c_R397::p_Init3(){
	gc_assign(m__consts,Array<c_ConstInfo* >(2));
	gc_assign(m__consts[0],(new c_ConstInfo)->m_new(String(L"RED",3),2,bb_reflection__intClass,((new c_IntObject)->m_new(-1))));
	gc_assign(m__consts[1],(new c_ConstInfo)->m_new(String(L"BLACK",5),2,bb_reflection__intClass,((new c_IntObject)->m_new(1))));
	gc_assign(m__fields,Array<c_FieldInfo* >(1));
	gc_assign(m__fields[0],((new c_R421)->m_new()));
	gc_assign(m__methods,Array<c_MethodInfo* >(23));
	gc_assign(m__methods[0],((new c_R398)->m_new()));
	gc_assign(m__methods[1],((new c_R399)->m_new()));
	gc_assign(m__methods[2],((new c_R400)->m_new()));
	gc_assign(m__methods[3],((new c_R401)->m_new()));
	gc_assign(m__methods[4],((new c_R402)->m_new()));
	gc_assign(m__methods[5],((new c_R403)->m_new()));
	gc_assign(m__methods[6],((new c_R404)->m_new()));
	gc_assign(m__methods[7],((new c_R405)->m_new()));
	gc_assign(m__methods[8],((new c_R406)->m_new()));
	gc_assign(m__methods[9],((new c_R407)->m_new()));
	gc_assign(m__methods[10],((new c_R408)->m_new()));
	gc_assign(m__methods[11],((new c_R409)->m_new()));
	gc_assign(m__methods[12],((new c_R410)->m_new()));
	gc_assign(m__methods[13],((new c_R411)->m_new()));
	gc_assign(m__methods[14],((new c_R412)->m_new()));
	gc_assign(m__methods[15],((new c_R413)->m_new()));
	gc_assign(m__methods[16],((new c_R414)->m_new()));
	gc_assign(m__methods[17],((new c_R415)->m_new()));
	gc_assign(m__methods[18],((new c_R416)->m_new()));
	gc_assign(m__methods[19],((new c_R417)->m_new()));
	gc_assign(m__methods[20],((new c_R418)->m_new()));
	gc_assign(m__methods[21],((new c_R419)->m_new()));
	gc_assign(m__methods[22],((new c_R420)->m_new()));
	gc_assign(m__ctors,Array<c_FunctionInfo* >(1));
	gc_assign(m__ctors[0],((new c_R422)->m_new()));
	p_InitR();
	return 0;
}
void c_R397::mark(){
	c_ClassInfo::mark();
}
c_R423::c_R423(){
}
c_R423* c_R423::m_new(){
	c_ClassInfo::m_new(String(L"monkey.map.FloatMap<monkey.lang.Object>",39),0,bb_reflection__classes[31],Array<c_ClassInfo* >());
	return this;
}
int c_R423::p_Init3(){
	gc_assign(m__methods,Array<c_MethodInfo* >(1));
	gc_assign(m__methods[0],((new c_R424)->m_new()));
	gc_assign(m__ctors,Array<c_FunctionInfo* >(1));
	gc_assign(m__ctors[0],((new c_R425)->m_new()));
	p_InitR();
	return 0;
}
void c_R423::mark(){
	c_ClassInfo::mark();
}
c_R426::c_R426(){
}
c_R426* c_R426::m_new(){
	c_ClassInfo::m_new(String(L"monkey.set.Set<String>",22),0,bb_reflection__classes[0],Array<c_ClassInfo* >());
	return this;
}
int c_R426::p_Init3(){
	gc_assign(m__fields,Array<c_FieldInfo* >(1));
	gc_assign(m__fields[0],((new c_R435)->m_new()));
	gc_assign(m__methods,Array<c_MethodInfo* >(7));
	gc_assign(m__methods[0],((new c_R428)->m_new()));
	gc_assign(m__methods[1],((new c_R429)->m_new()));
	gc_assign(m__methods[2],((new c_R430)->m_new()));
	gc_assign(m__methods[3],((new c_R431)->m_new()));
	gc_assign(m__methods[4],((new c_R432)->m_new()));
	gc_assign(m__methods[5],((new c_R433)->m_new()));
	gc_assign(m__methods[6],((new c_R434)->m_new()));
	gc_assign(m__ctors,Array<c_FunctionInfo* >(2));
	gc_assign(m__ctors[0],((new c_R427)->m_new()));
	gc_assign(m__ctors[1],((new c_R436)->m_new()));
	p_InitR();
	return 0;
}
void c_R426::mark(){
	c_ClassInfo::mark();
}
c_R437::c_R437(){
}
c_R437* c_R437::m_new(){
	c_ClassInfo::m_new(String(L"monkey.set.StringSet",20),0,bb_reflection__classes[33],Array<c_ClassInfo* >());
	return this;
}
int c_R437::p_Init3(){
	gc_assign(m__ctors,Array<c_FunctionInfo* >(1));
	gc_assign(m__ctors[0],((new c_R438)->m_new()));
	p_InitR();
	return 0;
}
void c_R437::mark(){
	c_ClassInfo::mark();
}
c_R439::c_R439(){
}
c_R439* c_R439::m_new(){
	c_ClassInfo::m_new(String(L"monkey.map.Map<String,monkey.lang.Object>",41),4,bb_reflection__classes[0],Array<c_ClassInfo* >());
	return this;
}
int c_R439::p_Init3(){
	gc_assign(m__consts,Array<c_ConstInfo* >(2));
	gc_assign(m__consts[0],(new c_ConstInfo)->m_new(String(L"RED",3),2,bb_reflection__intClass,((new c_IntObject)->m_new(-1))));
	gc_assign(m__consts[1],(new c_ConstInfo)->m_new(String(L"BLACK",5),2,bb_reflection__intClass,((new c_IntObject)->m_new(1))));
	gc_assign(m__fields,Array<c_FieldInfo* >(1));
	gc_assign(m__fields[0],((new c_R463)->m_new()));
	gc_assign(m__methods,Array<c_MethodInfo* >(23));
	gc_assign(m__methods[0],((new c_R440)->m_new()));
	gc_assign(m__methods[1],((new c_R441)->m_new()));
	gc_assign(m__methods[2],((new c_R442)->m_new()));
	gc_assign(m__methods[3],((new c_R443)->m_new()));
	gc_assign(m__methods[4],((new c_R444)->m_new()));
	gc_assign(m__methods[5],((new c_R445)->m_new()));
	gc_assign(m__methods[6],((new c_R446)->m_new()));
	gc_assign(m__methods[7],((new c_R447)->m_new()));
	gc_assign(m__methods[8],((new c_R448)->m_new()));
	gc_assign(m__methods[9],((new c_R449)->m_new()));
	gc_assign(m__methods[10],((new c_R450)->m_new()));
	gc_assign(m__methods[11],((new c_R451)->m_new()));
	gc_assign(m__methods[12],((new c_R452)->m_new()));
	gc_assign(m__methods[13],((new c_R453)->m_new()));
	gc_assign(m__methods[14],((new c_R454)->m_new()));
	gc_assign(m__methods[15],((new c_R455)->m_new()));
	gc_assign(m__methods[16],((new c_R456)->m_new()));
	gc_assign(m__methods[17],((new c_R457)->m_new()));
	gc_assign(m__methods[18],((new c_R458)->m_new()));
	gc_assign(m__methods[19],((new c_R459)->m_new()));
	gc_assign(m__methods[20],((new c_R460)->m_new()));
	gc_assign(m__methods[21],((new c_R461)->m_new()));
	gc_assign(m__methods[22],((new c_R462)->m_new()));
	gc_assign(m__ctors,Array<c_FunctionInfo* >(1));
	gc_assign(m__ctors[0],((new c_R464)->m_new()));
	p_InitR();
	return 0;
}
void c_R439::mark(){
	c_ClassInfo::mark();
}
c_R465::c_R465(){
}
c_R465* c_R465::m_new(){
	c_ClassInfo::m_new(String(L"monkey.map.StringMap<monkey.lang.Object>",40),0,bb_reflection__classes[35],Array<c_ClassInfo* >());
	return this;
}
int c_R465::p_Init3(){
	gc_assign(m__methods,Array<c_MethodInfo* >(1));
	gc_assign(m__methods[0],((new c_R466)->m_new()));
	gc_assign(m__ctors,Array<c_FunctionInfo* >(1));
	gc_assign(m__ctors[0],((new c_R467)->m_new()));
	p_InitR();
	return 0;
}
void c_R465::mark(){
	c_ClassInfo::mark();
}
c_R468::c_R468(){
}
c_R468* c_R468::m_new(){
	c_ClassInfo::m_new(String(L"monkey.stack.Stack<Int>",23),0,bb_reflection__classes[0],Array<c_ClassInfo* >());
	return this;
}
int c_R468::p_Init3(){
	gc_assign(m__globals,Array<c_GlobalInfo* >(1));
	gc_assign(m__globals[0],((new c_R497)->m_new()));
	gc_assign(m__fields,Array<c_FieldInfo* >(2));
	gc_assign(m__fields[0],((new c_R498)->m_new()));
	gc_assign(m__fields[1],((new c_R499)->m_new()));
	gc_assign(m__methods,Array<c_MethodInfo* >(31));
	gc_assign(m__methods[0],((new c_R471)->m_new()));
	gc_assign(m__methods[1],((new c_R472)->m_new()));
	gc_assign(m__methods[2],((new c_R473)->m_new()));
	gc_assign(m__methods[3],((new c_R474)->m_new()));
	gc_assign(m__methods[4],((new c_R475)->m_new()));
	gc_assign(m__methods[5],((new c_R476)->m_new()));
	gc_assign(m__methods[6],((new c_R477)->m_new()));
	gc_assign(m__methods[7],((new c_R478)->m_new()));
	gc_assign(m__methods[8],((new c_R479)->m_new()));
	gc_assign(m__methods[9],((new c_R480)->m_new()));
	gc_assign(m__methods[10],((new c_R481)->m_new()));
	gc_assign(m__methods[11],((new c_R482)->m_new()));
	gc_assign(m__methods[12],((new c_R483)->m_new()));
	gc_assign(m__methods[13],((new c_R484)->m_new()));
	gc_assign(m__methods[14],((new c_R485)->m_new()));
	gc_assign(m__methods[15],((new c_R486)->m_new()));
	gc_assign(m__methods[16],((new c_R487)->m_new()));
	gc_assign(m__methods[17],((new c_R488)->m_new()));
	gc_assign(m__methods[18],((new c_R489)->m_new()));
	gc_assign(m__methods[19],((new c_R490)->m_new()));
	gc_assign(m__methods[20],((new c_R491)->m_new()));
	gc_assign(m__methods[21],((new c_R492)->m_new()));
	gc_assign(m__methods[22],((new c_R493)->m_new()));
	gc_assign(m__methods[23],((new c_R494)->m_new()));
	gc_assign(m__methods[24],((new c_R495)->m_new()));
	gc_assign(m__methods[25],((new c_R496)->m_new()));
	gc_assign(m__methods[26],((new c_R500)->m_new()));
	gc_assign(m__methods[27],((new c_R501)->m_new()));
	gc_assign(m__methods[28],((new c_R502)->m_new()));
	gc_assign(m__methods[29],((new c_R503)->m_new()));
	gc_assign(m__methods[30],((new c_R504)->m_new()));
	gc_assign(m__ctors,Array<c_FunctionInfo* >(2));
	gc_assign(m__ctors[0],((new c_R469)->m_new()));
	gc_assign(m__ctors[1],((new c_R470)->m_new()));
	p_InitR();
	return 0;
}
void c_R468::mark(){
	c_ClassInfo::mark();
}
c_R505::c_R505(){
}
c_R505* c_R505::m_new(){
	c_ClassInfo::m_new(String(L"monkey.stack.IntStack",21),0,bb_reflection__classes[37],Array<c_ClassInfo* >());
	return this;
}
int c_R505::p_Init3(){
	gc_assign(m__methods,Array<c_MethodInfo* >(2));
	gc_assign(m__methods[0],((new c_R507)->m_new()));
	gc_assign(m__methods[1],((new c_R508)->m_new()));
	gc_assign(m__ctors,Array<c_FunctionInfo* >(2));
	gc_assign(m__ctors[0],((new c_R506)->m_new()));
	gc_assign(m__ctors[1],((new c_R509)->m_new()));
	p_InitR();
	return 0;
}
void c_R505::mark(){
	c_ClassInfo::mark();
}
c_R510::c_R510(){
}
c_R510* c_R510::m_new(){
	c_ClassInfo::m_new(String(L"monkey.stack.Stack<Float>",25),0,bb_reflection__classes[0],Array<c_ClassInfo* >());
	return this;
}
int c_R510::p_Init3(){
	gc_assign(m__globals,Array<c_GlobalInfo* >(1));
	gc_assign(m__globals[0],((new c_R539)->m_new()));
	gc_assign(m__fields,Array<c_FieldInfo* >(2));
	gc_assign(m__fields[0],((new c_R540)->m_new()));
	gc_assign(m__fields[1],((new c_R541)->m_new()));
	gc_assign(m__methods,Array<c_MethodInfo* >(31));
	gc_assign(m__methods[0],((new c_R513)->m_new()));
	gc_assign(m__methods[1],((new c_R514)->m_new()));
	gc_assign(m__methods[2],((new c_R515)->m_new()));
	gc_assign(m__methods[3],((new c_R516)->m_new()));
	gc_assign(m__methods[4],((new c_R517)->m_new()));
	gc_assign(m__methods[5],((new c_R518)->m_new()));
	gc_assign(m__methods[6],((new c_R519)->m_new()));
	gc_assign(m__methods[7],((new c_R520)->m_new()));
	gc_assign(m__methods[8],((new c_R521)->m_new()));
	gc_assign(m__methods[9],((new c_R522)->m_new()));
	gc_assign(m__methods[10],((new c_R523)->m_new()));
	gc_assign(m__methods[11],((new c_R524)->m_new()));
	gc_assign(m__methods[12],((new c_R525)->m_new()));
	gc_assign(m__methods[13],((new c_R526)->m_new()));
	gc_assign(m__methods[14],((new c_R527)->m_new()));
	gc_assign(m__methods[15],((new c_R528)->m_new()));
	gc_assign(m__methods[16],((new c_R529)->m_new()));
	gc_assign(m__methods[17],((new c_R530)->m_new()));
	gc_assign(m__methods[18],((new c_R531)->m_new()));
	gc_assign(m__methods[19],((new c_R532)->m_new()));
	gc_assign(m__methods[20],((new c_R533)->m_new()));
	gc_assign(m__methods[21],((new c_R534)->m_new()));
	gc_assign(m__methods[22],((new c_R535)->m_new()));
	gc_assign(m__methods[23],((new c_R536)->m_new()));
	gc_assign(m__methods[24],((new c_R537)->m_new()));
	gc_assign(m__methods[25],((new c_R538)->m_new()));
	gc_assign(m__methods[26],((new c_R542)->m_new()));
	gc_assign(m__methods[27],((new c_R543)->m_new()));
	gc_assign(m__methods[28],((new c_R544)->m_new()));
	gc_assign(m__methods[29],((new c_R545)->m_new()));
	gc_assign(m__methods[30],((new c_R546)->m_new()));
	gc_assign(m__ctors,Array<c_FunctionInfo* >(2));
	gc_assign(m__ctors[0],((new c_R511)->m_new()));
	gc_assign(m__ctors[1],((new c_R512)->m_new()));
	p_InitR();
	return 0;
}
void c_R510::mark(){
	c_ClassInfo::mark();
}
c_R547::c_R547(){
}
c_R547* c_R547::m_new(){
	c_ClassInfo::m_new(String(L"monkey.stack.FloatStack",23),0,bb_reflection__classes[39],Array<c_ClassInfo* >());
	return this;
}
int c_R547::p_Init3(){
	gc_assign(m__methods,Array<c_MethodInfo* >(2));
	gc_assign(m__methods[0],((new c_R549)->m_new()));
	gc_assign(m__methods[1],((new c_R550)->m_new()));
	gc_assign(m__ctors,Array<c_FunctionInfo* >(2));
	gc_assign(m__ctors[0],((new c_R548)->m_new()));
	gc_assign(m__ctors[1],((new c_R551)->m_new()));
	p_InitR();
	return 0;
}
void c_R547::mark(){
	c_ClassInfo::mark();
}
c_R552::c_R552(){
}
c_R552* c_R552::m_new(){
	c_ClassInfo::m_new(String(L"monkey.stack.Stack<String>",26),0,bb_reflection__classes[0],Array<c_ClassInfo* >());
	return this;
}
int c_R552::p_Init3(){
	gc_assign(m__globals,Array<c_GlobalInfo* >(1));
	gc_assign(m__globals[0],((new c_R581)->m_new()));
	gc_assign(m__fields,Array<c_FieldInfo* >(2));
	gc_assign(m__fields[0],((new c_R582)->m_new()));
	gc_assign(m__fields[1],((new c_R583)->m_new()));
	gc_assign(m__methods,Array<c_MethodInfo* >(31));
	gc_assign(m__methods[0],((new c_R555)->m_new()));
	gc_assign(m__methods[1],((new c_R556)->m_new()));
	gc_assign(m__methods[2],((new c_R557)->m_new()));
	gc_assign(m__methods[3],((new c_R558)->m_new()));
	gc_assign(m__methods[4],((new c_R559)->m_new()));
	gc_assign(m__methods[5],((new c_R560)->m_new()));
	gc_assign(m__methods[6],((new c_R561)->m_new()));
	gc_assign(m__methods[7],((new c_R562)->m_new()));
	gc_assign(m__methods[8],((new c_R563)->m_new()));
	gc_assign(m__methods[9],((new c_R564)->m_new()));
	gc_assign(m__methods[10],((new c_R565)->m_new()));
	gc_assign(m__methods[11],((new c_R566)->m_new()));
	gc_assign(m__methods[12],((new c_R567)->m_new()));
	gc_assign(m__methods[13],((new c_R568)->m_new()));
	gc_assign(m__methods[14],((new c_R569)->m_new()));
	gc_assign(m__methods[15],((new c_R570)->m_new()));
	gc_assign(m__methods[16],((new c_R571)->m_new()));
	gc_assign(m__methods[17],((new c_R572)->m_new()));
	gc_assign(m__methods[18],((new c_R573)->m_new()));
	gc_assign(m__methods[19],((new c_R574)->m_new()));
	gc_assign(m__methods[20],((new c_R575)->m_new()));
	gc_assign(m__methods[21],((new c_R576)->m_new()));
	gc_assign(m__methods[22],((new c_R577)->m_new()));
	gc_assign(m__methods[23],((new c_R578)->m_new()));
	gc_assign(m__methods[24],((new c_R579)->m_new()));
	gc_assign(m__methods[25],((new c_R580)->m_new()));
	gc_assign(m__methods[26],((new c_R584)->m_new()));
	gc_assign(m__methods[27],((new c_R585)->m_new()));
	gc_assign(m__methods[28],((new c_R586)->m_new()));
	gc_assign(m__methods[29],((new c_R587)->m_new()));
	gc_assign(m__methods[30],((new c_R588)->m_new()));
	gc_assign(m__ctors,Array<c_FunctionInfo* >(2));
	gc_assign(m__ctors[0],((new c_R553)->m_new()));
	gc_assign(m__ctors[1],((new c_R554)->m_new()));
	p_InitR();
	return 0;
}
void c_R552::mark(){
	c_ClassInfo::mark();
}
c_R589::c_R589(){
}
c_R589* c_R589::m_new(){
	c_ClassInfo::m_new(String(L"monkey.stack.StringStack",24),0,bb_reflection__classes[41],Array<c_ClassInfo* >());
	return this;
}
int c_R589::p_Init3(){
	gc_assign(m__methods,Array<c_MethodInfo* >(3));
	gc_assign(m__methods[0],((new c_R591)->m_new()));
	gc_assign(m__methods[1],((new c_R592)->m_new()));
	gc_assign(m__methods[2],((new c_R593)->m_new()));
	gc_assign(m__ctors,Array<c_FunctionInfo* >(2));
	gc_assign(m__ctors[0],((new c_R590)->m_new()));
	gc_assign(m__ctors[1],((new c_R594)->m_new()));
	p_InitR();
	return 0;
}
void c_R589::mark(){
	c_ClassInfo::mark();
}
c_R595::c_R595(){
}
c_R595* c_R595::m_new(){
	c_ClassInfo::m_new(String(L"monkeytarget.GlfwVideoMode",26),1,bb_reflection__classes[0],Array<c_ClassInfo* >());
	return this;
}
int c_R595::p_Init3(){
	gc_assign(m__fields,Array<c_FieldInfo* >(5));
	gc_assign(m__fields[0],((new c_R596)->m_new()));
	gc_assign(m__fields[1],((new c_R597)->m_new()));
	gc_assign(m__fields[2],((new c_R598)->m_new()));
	gc_assign(m__fields[3],((new c_R599)->m_new()));
	gc_assign(m__fields[4],((new c_R600)->m_new()));
	p_InitR();
	return 0;
}
void c_R595::mark(){
	c_ClassInfo::mark();
}
c_R601::c_R601(){
}
c_R601* c_R601::m_new(){
	c_ClassInfo::m_new(String(L"coregfx.color.Color",19),0,bb_reflection__classes[0],Array<c_ClassInfo* >());
	return this;
}
int c_R601::p_Init3(){
	gc_assign(m__globals,Array<c_GlobalInfo* >(21));
	gc_assign(m__globals[0],((new c_R606)->m_new()));
	gc_assign(m__globals[1],((new c_R607)->m_new()));
	gc_assign(m__globals[2],((new c_R608)->m_new()));
	gc_assign(m__globals[3],((new c_R609)->m_new()));
	gc_assign(m__globals[4],((new c_R610)->m_new()));
	gc_assign(m__globals[5],((new c_R611)->m_new()));
	gc_assign(m__globals[6],((new c_R612)->m_new()));
	gc_assign(m__globals[7],((new c_R613)->m_new()));
	gc_assign(m__globals[8],((new c_R614)->m_new()));
	gc_assign(m__globals[9],((new c_R615)->m_new()));
	gc_assign(m__globals[10],((new c_R616)->m_new()));
	gc_assign(m__globals[11],((new c_R617)->m_new()));
	gc_assign(m__globals[12],((new c_R618)->m_new()));
	gc_assign(m__globals[13],((new c_R619)->m_new()));
	gc_assign(m__globals[14],((new c_R620)->m_new()));
	gc_assign(m__globals[15],((new c_R621)->m_new()));
	gc_assign(m__globals[16],((new c_R622)->m_new()));
	gc_assign(m__globals[17],((new c_R623)->m_new()));
	gc_assign(m__globals[18],((new c_R624)->m_new()));
	gc_assign(m__globals[19],((new c_R625)->m_new()));
	gc_assign(m__globals[20],((new c_R626)->m_new()));
	gc_assign(m__fields,Array<c_FieldInfo* >(4));
	gc_assign(m__fields[0],((new c_R602)->m_new()));
	gc_assign(m__fields[1],((new c_R603)->m_new()));
	gc_assign(m__fields[2],((new c_R604)->m_new()));
	gc_assign(m__fields[3],((new c_R605)->m_new()));
	gc_assign(m__methods,Array<c_MethodInfo* >(20));
	gc_assign(m__methods[0],((new c_R630)->m_new()));
	gc_assign(m__methods[1],((new c_R631)->m_new()));
	gc_assign(m__methods[2],((new c_R632)->m_new()));
	gc_assign(m__methods[3],((new c_R633)->m_new()));
	gc_assign(m__methods[4],((new c_R634)->m_new()));
	gc_assign(m__methods[5],((new c_R635)->m_new()));
	gc_assign(m__methods[6],((new c_R636)->m_new()));
	gc_assign(m__methods[7],((new c_R637)->m_new()));
	gc_assign(m__methods[8],((new c_R638)->m_new()));
	gc_assign(m__methods[9],((new c_R639)->m_new()));
	gc_assign(m__methods[10],((new c_R640)->m_new()));
	gc_assign(m__methods[11],((new c_R641)->m_new()));
	gc_assign(m__methods[12],((new c_R642)->m_new()));
	gc_assign(m__methods[13],((new c_R643)->m_new()));
	gc_assign(m__methods[14],((new c_R644)->m_new()));
	gc_assign(m__methods[15],((new c_R645)->m_new()));
	gc_assign(m__methods[16],((new c_R646)->m_new()));
	gc_assign(m__methods[17],((new c_R647)->m_new()));
	gc_assign(m__methods[18],((new c_R648)->m_new()));
	gc_assign(m__methods[19],((new c_R649)->m_new()));
	gc_assign(m__ctors,Array<c_FunctionInfo* >(4));
	gc_assign(m__ctors[0],((new c_R627)->m_new()));
	gc_assign(m__ctors[1],((new c_R628)->m_new()));
	gc_assign(m__ctors[2],((new c_R629)->m_new()));
	gc_assign(m__ctors[3],((new c_R650)->m_new()));
	p_InitR();
	return 0;
}
void c_R601::mark(){
	c_ClassInfo::mark();
}
c_R651::c_R651(){
}
c_R651* c_R651::m_new(){
	c_ClassInfo::m_new(String(L"coregfx.color.ImmutableColor",28),0,bb_reflection__classes[44],Array<c_ClassInfo* >());
	return this;
}
int c_R651::p_Init3(){
	gc_assign(m__methods,Array<c_MethodInfo* >(10));
	gc_assign(m__methods[0],((new c_R655)->m_new()));
	gc_assign(m__methods[1],((new c_R656)->m_new()));
	gc_assign(m__methods[2],((new c_R657)->m_new()));
	gc_assign(m__methods[3],((new c_R658)->m_new()));
	gc_assign(m__methods[4],((new c_R659)->m_new()));
	gc_assign(m__methods[5],((new c_R660)->m_new()));
	gc_assign(m__methods[6],((new c_R661)->m_new()));
	gc_assign(m__methods[7],((new c_R662)->m_new()));
	gc_assign(m__methods[8],((new c_R663)->m_new()));
	gc_assign(m__methods[9],((new c_R664)->m_new()));
	gc_assign(m__ctors,Array<c_FunctionInfo* >(3));
	gc_assign(m__ctors[0],((new c_R652)->m_new()));
	gc_assign(m__ctors[1],((new c_R653)->m_new()));
	gc_assign(m__ctors[2],((new c_R654)->m_new()));
	p_InitR();
	return 0;
}
void c_R651::mark(){
	c_ClassInfo::mark();
}
c_R665::c_R665(){
}
c_R665* c_R665::m_new(){
	c_ClassInfo::m_new(String(L"foundation.vec2.Vec2",20),8,bb_reflection__classes[0],Array<c_ClassInfo* >());
	return this;
}
int c_R665::p_Init3(){
	gc_assign(m__fields,Array<c_FieldInfo* >(2));
	gc_assign(m__fields[0],((new c_R666)->m_new()));
	gc_assign(m__fields[1],((new c_R667)->m_new()));
	gc_assign(m__methods,Array<c_MethodInfo* >(23));
	gc_assign(m__methods[0],((new c_R670)->m_new()));
	gc_assign(m__methods[1],((new c_R672)->m_new()));
	gc_assign(m__methods[2],((new c_R673)->m_new()));
	gc_assign(m__methods[3],((new c_R674)->m_new()));
	gc_assign(m__methods[4],((new c_R675)->m_new()));
	gc_assign(m__methods[5],((new c_R676)->m_new()));
	gc_assign(m__methods[6],((new c_R677)->m_new()));
	gc_assign(m__methods[7],((new c_R678)->m_new()));
	gc_assign(m__methods[8],((new c_R679)->m_new()));
	gc_assign(m__methods[9],((new c_R680)->m_new()));
	gc_assign(m__methods[10],((new c_R681)->m_new()));
	gc_assign(m__methods[11],((new c_R682)->m_new()));
	gc_assign(m__methods[12],((new c_R683)->m_new()));
	gc_assign(m__methods[13],((new c_R684)->m_new()));
	gc_assign(m__methods[14],((new c_R685)->m_new()));
	gc_assign(m__methods[15],((new c_R686)->m_new()));
	gc_assign(m__methods[16],((new c_R687)->m_new()));
	gc_assign(m__methods[17],((new c_R688)->m_new()));
	gc_assign(m__methods[18],((new c_R689)->m_new()));
	gc_assign(m__methods[19],((new c_R690)->m_new()));
	gc_assign(m__methods[20],((new c_R691)->m_new()));
	gc_assign(m__methods[21],((new c_R692)->m_new()));
	gc_assign(m__methods[22],((new c_R693)->m_new()));
	gc_assign(m__functions,Array<c_FunctionInfo* >(11));
	gc_assign(m__functions[0],((new c_R671)->m_new()));
	gc_assign(m__functions[1],((new c_R694)->m_new()));
	gc_assign(m__functions[2],((new c_R695)->m_new()));
	gc_assign(m__functions[3],((new c_R696)->m_new()));
	gc_assign(m__functions[4],((new c_R697)->m_new()));
	gc_assign(m__functions[5],((new c_R698)->m_new()));
	gc_assign(m__functions[6],((new c_R699)->m_new()));
	gc_assign(m__functions[7],((new c_R700)->m_new()));
	gc_assign(m__functions[8],((new c_R701)->m_new()));
	gc_assign(m__functions[9],((new c_R702)->m_new()));
	gc_assign(m__functions[10],((new c_R703)->m_new()));
	gc_assign(m__ctors,Array<c_FunctionInfo* >(3));
	gc_assign(m__ctors[0],((new c_R668)->m_new()));
	gc_assign(m__ctors[1],((new c_R669)->m_new()));
	gc_assign(m__ctors[2],((new c_R704)->m_new()));
	p_InitR();
	return 0;
}
void c_R665::mark(){
	c_ClassInfo::mark();
}
c_R705::c_R705(){
}
c_R705* c_R705::m_new(){
	c_ClassInfo::m_new(String(L"framework.entity.VEntity",24),4,bb_reflection__classes[0],Array<c_ClassInfo* >());
	return this;
}
int c_R705::p_Init3(){
	gc_assign(m__fields,Array<c_FieldInfo* >(4));
	gc_assign(m__fields[0],((new c_R706)->m_new()));
	gc_assign(m__fields[1],((new c_R707)->m_new()));
	gc_assign(m__fields[2],((new c_R708)->m_new()));
	gc_assign(m__fields[3],((new c_R709)->m_new()));
	gc_assign(m__methods,Array<c_MethodInfo* >(11));
	gc_assign(m__methods[0],((new c_R710)->m_new()));
	gc_assign(m__methods[1],((new c_R711)->m_new()));
	gc_assign(m__methods[2],((new c_R712)->m_new()));
	gc_assign(m__methods[3],((new c_R713)->m_new()));
	gc_assign(m__methods[4],((new c_R714)->m_new()));
	gc_assign(m__methods[5],((new c_R715)->m_new()));
	gc_assign(m__methods[6],((new c_R716)->m_new()));
	gc_assign(m__methods[7],((new c_R717)->m_new()));
	gc_assign(m__methods[8],((new c_R718)->m_new()));
	gc_assign(m__methods[9],((new c_R719)->m_new()));
	gc_assign(m__methods[10],((new c_R720)->m_new()));
	gc_assign(m__ctors,Array<c_FunctionInfo* >(1));
	gc_assign(m__ctors[0],((new c_R721)->m_new()));
	p_InitR();
	return 0;
}
void c_R705::mark(){
	c_ClassInfo::mark();
}
c_R722::c_R722(){
}
c_R722* c_R722::m_new(){
	c_ClassInfo::m_new(String(L"monkey.map.Map<String,String>",29),4,bb_reflection__classes[0],Array<c_ClassInfo* >());
	return this;
}
int c_R722::p_Init3(){
	gc_assign(m__consts,Array<c_ConstInfo* >(2));
	gc_assign(m__consts[0],(new c_ConstInfo)->m_new(String(L"RED",3),2,bb_reflection__intClass,((new c_IntObject)->m_new(-1))));
	gc_assign(m__consts[1],(new c_ConstInfo)->m_new(String(L"BLACK",5),2,bb_reflection__intClass,((new c_IntObject)->m_new(1))));
	gc_assign(m__fields,Array<c_FieldInfo* >(1));
	gc_assign(m__fields[0],((new c_R746)->m_new()));
	gc_assign(m__methods,Array<c_MethodInfo* >(23));
	gc_assign(m__methods[0],((new c_R723)->m_new()));
	gc_assign(m__methods[1],((new c_R724)->m_new()));
	gc_assign(m__methods[2],((new c_R725)->m_new()));
	gc_assign(m__methods[3],((new c_R726)->m_new()));
	gc_assign(m__methods[4],((new c_R727)->m_new()));
	gc_assign(m__methods[5],((new c_R728)->m_new()));
	gc_assign(m__methods[6],((new c_R729)->m_new()));
	gc_assign(m__methods[7],((new c_R730)->m_new()));
	gc_assign(m__methods[8],((new c_R731)->m_new()));
	gc_assign(m__methods[9],((new c_R732)->m_new()));
	gc_assign(m__methods[10],((new c_R733)->m_new()));
	gc_assign(m__methods[11],((new c_R734)->m_new()));
	gc_assign(m__methods[12],((new c_R735)->m_new()));
	gc_assign(m__methods[13],((new c_R736)->m_new()));
	gc_assign(m__methods[14],((new c_R737)->m_new()));
	gc_assign(m__methods[15],((new c_R738)->m_new()));
	gc_assign(m__methods[16],((new c_R739)->m_new()));
	gc_assign(m__methods[17],((new c_R740)->m_new()));
	gc_assign(m__methods[18],((new c_R741)->m_new()));
	gc_assign(m__methods[19],((new c_R742)->m_new()));
	gc_assign(m__methods[20],((new c_R743)->m_new()));
	gc_assign(m__methods[21],((new c_R744)->m_new()));
	gc_assign(m__methods[22],((new c_R745)->m_new()));
	gc_assign(m__ctors,Array<c_FunctionInfo* >(1));
	gc_assign(m__ctors[0],((new c_R747)->m_new()));
	p_InitR();
	return 0;
}
void c_R722::mark(){
	c_ClassInfo::mark();
}
c_R748::c_R748(){
}
c_R748* c_R748::m_new(){
	c_ClassInfo::m_new(String(L"monkey.map.StringMap<String>",28),0,bb_reflection__classes[48],Array<c_ClassInfo* >());
	return this;
}
int c_R748::p_Init3(){
	gc_assign(m__methods,Array<c_MethodInfo* >(1));
	gc_assign(m__methods[0],((new c_R749)->m_new()));
	gc_assign(m__ctors,Array<c_FunctionInfo* >(1));
	gc_assign(m__ctors[0],((new c_R750)->m_new()));
	p_InitR();
	return 0;
}
void c_R748::mark(){
	c_ClassInfo::mark();
}
c_R751::c_R751(){
}
c_R751* c_R751::m_new(){
	c_ClassInfo::m_new(String(L"monkey.map.Node<String,String>",30),0,bb_reflection__classes[0],Array<c_ClassInfo* >());
	return this;
}
int c_R751::p_Init3(){
	gc_assign(m__fields,Array<c_FieldInfo* >(6));
	gc_assign(m__fields[0],((new c_R759)->m_new()));
	gc_assign(m__fields[1],((new c_R760)->m_new()));
	gc_assign(m__fields[2],((new c_R761)->m_new()));
	gc_assign(m__fields[3],((new c_R762)->m_new()));
	gc_assign(m__fields[4],((new c_R763)->m_new()));
	gc_assign(m__fields[5],((new c_R764)->m_new()));
	gc_assign(m__methods,Array<c_MethodInfo* >(6));
	gc_assign(m__methods[0],((new c_R753)->m_new()));
	gc_assign(m__methods[1],((new c_R754)->m_new()));
	gc_assign(m__methods[2],((new c_R755)->m_new()));
	gc_assign(m__methods[3],((new c_R756)->m_new()));
	gc_assign(m__methods[4],((new c_R757)->m_new()));
	gc_assign(m__methods[5],((new c_R758)->m_new()));
	gc_assign(m__ctors,Array<c_FunctionInfo* >(2));
	gc_assign(m__ctors[0],((new c_R752)->m_new()));
	gc_assign(m__ctors[1],((new c_R765)->m_new()));
	p_InitR();
	return 0;
}
void c_R751::mark(){
	c_ClassInfo::mark();
}
c_R766::c_R766(){
}
c_R766* c_R766::m_new(){
	c_ClassInfo::m_new(String(L"monkey.map.MapKeys<String,String>",33),0,bb_reflection__classes[0],Array<c_ClassInfo* >());
	return this;
}
int c_R766::p_Init3(){
	gc_assign(m__fields,Array<c_FieldInfo* >(1));
	gc_assign(m__fields[0],((new c_R769)->m_new()));
	gc_assign(m__methods,Array<c_MethodInfo* >(1));
	gc_assign(m__methods[0],((new c_R768)->m_new()));
	gc_assign(m__ctors,Array<c_FunctionInfo* >(2));
	gc_assign(m__ctors[0],((new c_R767)->m_new()));
	gc_assign(m__ctors[1],((new c_R770)->m_new()));
	p_InitR();
	return 0;
}
void c_R766::mark(){
	c_ClassInfo::mark();
}
c_R771::c_R771(){
}
c_R771* c_R771::m_new(){
	c_ClassInfo::m_new(String(L"monkey.map.KeyEnumerator<String,String>",39),0,bb_reflection__classes[0],Array<c_ClassInfo* >());
	return this;
}
int c_R771::p_Init3(){
	gc_assign(m__fields,Array<c_FieldInfo* >(1));
	gc_assign(m__fields[0],((new c_R775)->m_new()));
	gc_assign(m__methods,Array<c_MethodInfo* >(2));
	gc_assign(m__methods[0],((new c_R773)->m_new()));
	gc_assign(m__methods[1],((new c_R774)->m_new()));
	gc_assign(m__ctors,Array<c_FunctionInfo* >(2));
	gc_assign(m__ctors[0],((new c_R772)->m_new()));
	gc_assign(m__ctors[1],((new c_R776)->m_new()));
	p_InitR();
	return 0;
}
void c_R771::mark(){
	c_ClassInfo::mark();
}
c_R777::c_R777(){
}
c_R777* c_R777::m_new(){
	c_ClassInfo::m_new(String(L"framework.shapes.VShape",23),4,bb_reflection__classes[47],Array<c_ClassInfo* >());
	return this;
}
int c_R777::p_Init3(){
	gc_assign(m__fields,Array<c_FieldInfo* >(2));
	gc_assign(m__fields[0],((new c_R778)->m_new()));
	gc_assign(m__fields[1],((new c_R779)->m_new()));
	gc_assign(m__methods,Array<c_MethodInfo* >(8));
	gc_assign(m__methods[0],((new c_R780)->m_new()));
	gc_assign(m__methods[1],((new c_R781)->m_new()));
	gc_assign(m__methods[2],((new c_R782)->m_new()));
	gc_assign(m__methods[3],((new c_R783)->m_new()));
	gc_assign(m__methods[4],((new c_R784)->m_new()));
	gc_assign(m__methods[5],((new c_R785)->m_new()));
	gc_assign(m__methods[6],((new c_R786)->m_new()));
	gc_assign(m__methods[7],((new c_R787)->m_new()));
	gc_assign(m__ctors,Array<c_FunctionInfo* >(1));
	gc_assign(m__ctors[0],((new c_R788)->m_new()));
	p_InitR();
	return 0;
}
void c_R777::mark(){
	c_ClassInfo::mark();
}
c_R789::c_R789(){
}
c_R789* c_R789::m_new(){
	c_ClassInfo::m_new(String(L"framework.shapes.VRect",22),0,bb_reflection__classes[53],Array<c_ClassInfo* >());
	return this;
}
int c_R789::p_Init3(){
	gc_assign(m__fields,Array<c_FieldInfo* >(1));
	gc_assign(m__fields[0],((new c_R790)->m_new()));
	gc_assign(m__methods,Array<c_MethodInfo* >(15));
	gc_assign(m__methods[0],((new c_R793)->m_new()));
	gc_assign(m__methods[1],((new c_R794)->m_new()));
	gc_assign(m__methods[2],((new c_R795)->m_new()));
	gc_assign(m__methods[3],((new c_R796)->m_new()));
	gc_assign(m__methods[4],((new c_R797)->m_new()));
	gc_assign(m__methods[5],((new c_R798)->m_new()));
	gc_assign(m__methods[6],((new c_R799)->m_new()));
	gc_assign(m__methods[7],((new c_R800)->m_new()));
	gc_assign(m__methods[8],((new c_R801)->m_new()));
	gc_assign(m__methods[9],((new c_R802)->m_new()));
	gc_assign(m__methods[10],((new c_R803)->m_new()));
	gc_assign(m__methods[11],((new c_R804)->m_new()));
	gc_assign(m__methods[12],((new c_R805)->m_new()));
	gc_assign(m__methods[13],((new c_R806)->m_new()));
	gc_assign(m__methods[14],((new c_R807)->m_new()));
	gc_assign(m__ctors,Array<c_FunctionInfo* >(3));
	gc_assign(m__ctors[0],((new c_R791)->m_new()));
	gc_assign(m__ctors[1],((new c_R792)->m_new()));
	gc_assign(m__ctors[2],((new c_R808)->m_new()));
	p_InitR();
	return 0;
}
void c_R789::mark(){
	c_ClassInfo::mark();
}
c_R809::c_R809(){
}
c_R809* c_R809::m_new(){
	c_ClassInfo::m_new(String(L"framework.shapes.VCircle",24),0,bb_reflection__classes[53],Array<c_ClassInfo* >());
	return this;
}
int c_R809::p_Init3(){
	gc_assign(m__fields,Array<c_FieldInfo* >(1));
	gc_assign(m__fields[0],((new c_R810)->m_new()));
	gc_assign(m__methods,Array<c_MethodInfo* >(8));
	gc_assign(m__methods[0],((new c_R812)->m_new()));
	gc_assign(m__methods[1],((new c_R813)->m_new()));
	gc_assign(m__methods[2],((new c_R814)->m_new()));
	gc_assign(m__methods[3],((new c_R815)->m_new()));
	gc_assign(m__methods[4],((new c_R816)->m_new()));
	gc_assign(m__methods[5],((new c_R817)->m_new()));
	gc_assign(m__methods[6],((new c_R818)->m_new()));
	gc_assign(m__methods[7],((new c_R819)->m_new()));
	gc_assign(m__ctors,Array<c_FunctionInfo* >(2));
	gc_assign(m__ctors[0],((new c_R811)->m_new()));
	gc_assign(m__ctors[1],((new c_R820)->m_new()));
	p_InitR();
	return 0;
}
void c_R809::mark(){
	c_ClassInfo::mark();
}
c_R821::c_R821(){
}
c_R821* c_R821::m_new(){
	c_ClassInfo::m_new(String(L"framework.sprite.VSprite",24),0,bb_reflection__classes[47],Array<c_ClassInfo* >());
	return this;
}
int c_R821::p_Init3(){
	gc_assign(m__fields,Array<c_FieldInfo* >(5));
	gc_assign(m__fields[0],((new c_R822)->m_new()));
	gc_assign(m__fields[1],((new c_R823)->m_new()));
	gc_assign(m__fields[2],((new c_R824)->m_new()));
	gc_assign(m__fields[3],((new c_R825)->m_new()));
	gc_assign(m__fields[4],((new c_R841)->m_new()));
	gc_assign(m__methods,Array<c_MethodInfo* >(14));
	gc_assign(m__methods[0],((new c_R827)->m_new()));
	gc_assign(m__methods[1],((new c_R828)->m_new()));
	gc_assign(m__methods[2],((new c_R829)->m_new()));
	gc_assign(m__methods[3],((new c_R830)->m_new()));
	gc_assign(m__methods[4],((new c_R831)->m_new()));
	gc_assign(m__methods[5],((new c_R832)->m_new()));
	gc_assign(m__methods[6],((new c_R833)->m_new()));
	gc_assign(m__methods[7],((new c_R834)->m_new()));
	gc_assign(m__methods[8],((new c_R835)->m_new()));
	gc_assign(m__methods[9],((new c_R836)->m_new()));
	gc_assign(m__methods[10],((new c_R837)->m_new()));
	gc_assign(m__methods[11],((new c_R838)->m_new()));
	gc_assign(m__methods[12],((new c_R839)->m_new()));
	gc_assign(m__methods[13],((new c_R840)->m_new()));
	gc_assign(m__ctors,Array<c_FunctionInfo* >(2));
	gc_assign(m__ctors[0],((new c_R826)->m_new()));
	gc_assign(m__ctors[1],((new c_R842)->m_new()));
	p_InitR();
	return 0;
}
void c_R821::mark(){
	c_ClassInfo::mark();
}
c_R843::c_R843(){
}
c_R843* c_R843::m_new(){
	c_ClassInfo::m_new(String(L"monkey.deque.Enumerator<Int>",28),0,bb_reflection__classes[0],Array<c_ClassInfo* >());
	return this;
}
int c_R843::p_Init3(){
	gc_assign(m__fields,Array<c_FieldInfo* >(2));
	gc_assign(m__fields[0],((new c_R847)->m_new()));
	gc_assign(m__fields[1],((new c_R848)->m_new()));
	gc_assign(m__methods,Array<c_MethodInfo* >(2));
	gc_assign(m__methods[0],((new c_R845)->m_new()));
	gc_assign(m__methods[1],((new c_R846)->m_new()));
	gc_assign(m__ctors,Array<c_FunctionInfo* >(2));
	gc_assign(m__ctors[0],((new c_R844)->m_new()));
	gc_assign(m__ctors[1],((new c_R849)->m_new()));
	p_InitR();
	return 0;
}
void c_R843::mark(){
	c_ClassInfo::mark();
}
c_R850::c_R850(){
}
c_R850* c_R850::m_new(){
	c_ClassInfo::m_new(String(L"monkey.deque.Enumerator<Float>",30),0,bb_reflection__classes[0],Array<c_ClassInfo* >());
	return this;
}
int c_R850::p_Init3(){
	gc_assign(m__fields,Array<c_FieldInfo* >(2));
	gc_assign(m__fields[0],((new c_R854)->m_new()));
	gc_assign(m__fields[1],((new c_R855)->m_new()));
	gc_assign(m__methods,Array<c_MethodInfo* >(2));
	gc_assign(m__methods[0],((new c_R852)->m_new()));
	gc_assign(m__methods[1],((new c_R853)->m_new()));
	gc_assign(m__ctors,Array<c_FunctionInfo* >(2));
	gc_assign(m__ctors[0],((new c_R851)->m_new()));
	gc_assign(m__ctors[1],((new c_R856)->m_new()));
	p_InitR();
	return 0;
}
void c_R850::mark(){
	c_ClassInfo::mark();
}
c_R857::c_R857(){
}
c_R857* c_R857::m_new(){
	c_ClassInfo::m_new(String(L"monkey.deque.Enumerator<String>",31),0,bb_reflection__classes[0],Array<c_ClassInfo* >());
	return this;
}
int c_R857::p_Init3(){
	gc_assign(m__fields,Array<c_FieldInfo* >(2));
	gc_assign(m__fields[0],((new c_R861)->m_new()));
	gc_assign(m__fields[1],((new c_R862)->m_new()));
	gc_assign(m__methods,Array<c_MethodInfo* >(2));
	gc_assign(m__methods[0],((new c_R859)->m_new()));
	gc_assign(m__methods[1],((new c_R860)->m_new()));
	gc_assign(m__ctors,Array<c_FunctionInfo* >(2));
	gc_assign(m__ctors[0],((new c_R858)->m_new()));
	gc_assign(m__ctors[1],((new c_R863)->m_new()));
	p_InitR();
	return 0;
}
void c_R857::mark(){
	c_ClassInfo::mark();
}
c_R864::c_R864(){
}
c_R864* c_R864::m_new(){
	c_ClassInfo::m_new(String(L"monkey.list.Enumerator<Int>",27),0,bb_reflection__classes[0],Array<c_ClassInfo* >());
	return this;
}
int c_R864::p_Init3(){
	gc_assign(m__fields,Array<c_FieldInfo* >(2));
	gc_assign(m__fields[0],((new c_R868)->m_new()));
	gc_assign(m__fields[1],((new c_R869)->m_new()));
	gc_assign(m__methods,Array<c_MethodInfo* >(2));
	gc_assign(m__methods[0],((new c_R866)->m_new()));
	gc_assign(m__methods[1],((new c_R867)->m_new()));
	gc_assign(m__ctors,Array<c_FunctionInfo* >(2));
	gc_assign(m__ctors[0],((new c_R865)->m_new()));
	gc_assign(m__ctors[1],((new c_R870)->m_new()));
	p_InitR();
	return 0;
}
void c_R864::mark(){
	c_ClassInfo::mark();
}
c_R871::c_R871(){
}
c_R871* c_R871::m_new(){
	c_ClassInfo::m_new(String(L"monkey.list.BackwardsList<Int>",30),0,bb_reflection__classes[0],Array<c_ClassInfo* >());
	return this;
}
int c_R871::p_Init3(){
	gc_assign(m__fields,Array<c_FieldInfo* >(1));
	gc_assign(m__fields[0],((new c_R874)->m_new()));
	gc_assign(m__methods,Array<c_MethodInfo* >(1));
	gc_assign(m__methods[0],((new c_R873)->m_new()));
	gc_assign(m__ctors,Array<c_FunctionInfo* >(2));
	gc_assign(m__ctors[0],((new c_R872)->m_new()));
	gc_assign(m__ctors[1],((new c_R875)->m_new()));
	p_InitR();
	return 0;
}
void c_R871::mark(){
	c_ClassInfo::mark();
}
c_R876::c_R876(){
}
c_R876* c_R876::m_new(){
	c_ClassInfo::m_new(String(L"monkey.list.Enumerator<Float>",29),0,bb_reflection__classes[0],Array<c_ClassInfo* >());
	return this;
}
int c_R876::p_Init3(){
	gc_assign(m__fields,Array<c_FieldInfo* >(2));
	gc_assign(m__fields[0],((new c_R880)->m_new()));
	gc_assign(m__fields[1],((new c_R881)->m_new()));
	gc_assign(m__methods,Array<c_MethodInfo* >(2));
	gc_assign(m__methods[0],((new c_R878)->m_new()));
	gc_assign(m__methods[1],((new c_R879)->m_new()));
	gc_assign(m__ctors,Array<c_FunctionInfo* >(2));
	gc_assign(m__ctors[0],((new c_R877)->m_new()));
	gc_assign(m__ctors[1],((new c_R882)->m_new()));
	p_InitR();
	return 0;
}
void c_R876::mark(){
	c_ClassInfo::mark();
}
c_R883::c_R883(){
}
c_R883* c_R883::m_new(){
	c_ClassInfo::m_new(String(L"monkey.list.BackwardsList<Float>",32),0,bb_reflection__classes[0],Array<c_ClassInfo* >());
	return this;
}
int c_R883::p_Init3(){
	gc_assign(m__fields,Array<c_FieldInfo* >(1));
	gc_assign(m__fields[0],((new c_R886)->m_new()));
	gc_assign(m__methods,Array<c_MethodInfo* >(1));
	gc_assign(m__methods[0],((new c_R885)->m_new()));
	gc_assign(m__ctors,Array<c_FunctionInfo* >(2));
	gc_assign(m__ctors[0],((new c_R884)->m_new()));
	gc_assign(m__ctors[1],((new c_R887)->m_new()));
	p_InitR();
	return 0;
}
void c_R883::mark(){
	c_ClassInfo::mark();
}
c_R888::c_R888(){
}
c_R888* c_R888::m_new(){
	c_ClassInfo::m_new(String(L"monkey.list.BackwardsList<String>",33),0,bb_reflection__classes[0],Array<c_ClassInfo* >());
	return this;
}
int c_R888::p_Init3(){
	gc_assign(m__fields,Array<c_FieldInfo* >(1));
	gc_assign(m__fields[0],((new c_R891)->m_new()));
	gc_assign(m__methods,Array<c_MethodInfo* >(1));
	gc_assign(m__methods[0],((new c_R890)->m_new()));
	gc_assign(m__ctors,Array<c_FunctionInfo* >(2));
	gc_assign(m__ctors[0],((new c_R889)->m_new()));
	gc_assign(m__ctors[1],((new c_R892)->m_new()));
	p_InitR();
	return 0;
}
void c_R888::mark(){
	c_ClassInfo::mark();
}
c_R893::c_R893(){
}
c_R893* c_R893::m_new(){
	c_ClassInfo::m_new(String(L"monkey.list.BackwardsEnumerator<Int>",36),0,bb_reflection__classes[0],Array<c_ClassInfo* >());
	return this;
}
int c_R893::p_Init3(){
	gc_assign(m__fields,Array<c_FieldInfo* >(2));
	gc_assign(m__fields[0],((new c_R897)->m_new()));
	gc_assign(m__fields[1],((new c_R898)->m_new()));
	gc_assign(m__methods,Array<c_MethodInfo* >(2));
	gc_assign(m__methods[0],((new c_R895)->m_new()));
	gc_assign(m__methods[1],((new c_R896)->m_new()));
	gc_assign(m__ctors,Array<c_FunctionInfo* >(2));
	gc_assign(m__ctors[0],((new c_R894)->m_new()));
	gc_assign(m__ctors[1],((new c_R899)->m_new()));
	p_InitR();
	return 0;
}
void c_R893::mark(){
	c_ClassInfo::mark();
}
c_R900::c_R900(){
}
c_R900* c_R900::m_new(){
	c_ClassInfo::m_new(String(L"monkey.list.BackwardsEnumerator<Float>",38),0,bb_reflection__classes[0],Array<c_ClassInfo* >());
	return this;
}
int c_R900::p_Init3(){
	gc_assign(m__fields,Array<c_FieldInfo* >(2));
	gc_assign(m__fields[0],((new c_R904)->m_new()));
	gc_assign(m__fields[1],((new c_R905)->m_new()));
	gc_assign(m__methods,Array<c_MethodInfo* >(2));
	gc_assign(m__methods[0],((new c_R902)->m_new()));
	gc_assign(m__methods[1],((new c_R903)->m_new()));
	gc_assign(m__ctors,Array<c_FunctionInfo* >(2));
	gc_assign(m__ctors[0],((new c_R901)->m_new()));
	gc_assign(m__ctors[1],((new c_R906)->m_new()));
	p_InitR();
	return 0;
}
void c_R900::mark(){
	c_ClassInfo::mark();
}
c_R907::c_R907(){
}
c_R907* c_R907::m_new(){
	c_ClassInfo::m_new(String(L"monkey.list.BackwardsEnumerator<String>",39),0,bb_reflection__classes[0],Array<c_ClassInfo* >());
	return this;
}
int c_R907::p_Init3(){
	gc_assign(m__fields,Array<c_FieldInfo* >(2));
	gc_assign(m__fields[0],((new c_R911)->m_new()));
	gc_assign(m__fields[1],((new c_R912)->m_new()));
	gc_assign(m__methods,Array<c_MethodInfo* >(2));
	gc_assign(m__methods[0],((new c_R909)->m_new()));
	gc_assign(m__methods[1],((new c_R910)->m_new()));
	gc_assign(m__ctors,Array<c_FunctionInfo* >(2));
	gc_assign(m__ctors[0],((new c_R908)->m_new()));
	gc_assign(m__ctors[1],((new c_R913)->m_new()));
	p_InitR();
	return 0;
}
void c_R907::mark(){
	c_ClassInfo::mark();
}
c_R914::c_R914(){
}
c_R914* c_R914::m_new(){
	c_ClassInfo::m_new(String(L"monkey.map.Node<Int,monkey.lang.Object>",39),0,bb_reflection__classes[0],Array<c_ClassInfo* >());
	return this;
}
int c_R914::p_Init3(){
	gc_assign(m__fields,Array<c_FieldInfo* >(6));
	gc_assign(m__fields[0],((new c_R922)->m_new()));
	gc_assign(m__fields[1],((new c_R923)->m_new()));
	gc_assign(m__fields[2],((new c_R924)->m_new()));
	gc_assign(m__fields[3],((new c_R925)->m_new()));
	gc_assign(m__fields[4],((new c_R926)->m_new()));
	gc_assign(m__fields[5],((new c_R927)->m_new()));
	gc_assign(m__methods,Array<c_MethodInfo* >(6));
	gc_assign(m__methods[0],((new c_R916)->m_new()));
	gc_assign(m__methods[1],((new c_R917)->m_new()));
	gc_assign(m__methods[2],((new c_R918)->m_new()));
	gc_assign(m__methods[3],((new c_R919)->m_new()));
	gc_assign(m__methods[4],((new c_R920)->m_new()));
	gc_assign(m__methods[5],((new c_R921)->m_new()));
	gc_assign(m__ctors,Array<c_FunctionInfo* >(2));
	gc_assign(m__ctors[0],((new c_R915)->m_new()));
	gc_assign(m__ctors[1],((new c_R928)->m_new()));
	p_InitR();
	return 0;
}
void c_R914::mark(){
	c_ClassInfo::mark();
}
c_R929::c_R929(){
}
c_R929* c_R929::m_new(){
	c_ClassInfo::m_new(String(L"monkey.map.MapKeys<Int,monkey.lang.Object>",42),0,bb_reflection__classes[0],Array<c_ClassInfo* >());
	return this;
}
int c_R929::p_Init3(){
	gc_assign(m__fields,Array<c_FieldInfo* >(1));
	gc_assign(m__fields[0],((new c_R932)->m_new()));
	gc_assign(m__methods,Array<c_MethodInfo* >(1));
	gc_assign(m__methods[0],((new c_R931)->m_new()));
	gc_assign(m__ctors,Array<c_FunctionInfo* >(2));
	gc_assign(m__ctors[0],((new c_R930)->m_new()));
	gc_assign(m__ctors[1],((new c_R933)->m_new()));
	p_InitR();
	return 0;
}
void c_R929::mark(){
	c_ClassInfo::mark();
}
c_R934::c_R934(){
}
c_R934* c_R934::m_new(){
	c_ClassInfo::m_new(String(L"monkey.map.MapValues<Int,monkey.lang.Object>",44),0,bb_reflection__classes[0],Array<c_ClassInfo* >());
	return this;
}
int c_R934::p_Init3(){
	gc_assign(m__fields,Array<c_FieldInfo* >(1));
	gc_assign(m__fields[0],((new c_R937)->m_new()));
	gc_assign(m__methods,Array<c_MethodInfo* >(1));
	gc_assign(m__methods[0],((new c_R936)->m_new()));
	gc_assign(m__ctors,Array<c_FunctionInfo* >(2));
	gc_assign(m__ctors[0],((new c_R935)->m_new()));
	gc_assign(m__ctors[1],((new c_R938)->m_new()));
	p_InitR();
	return 0;
}
void c_R934::mark(){
	c_ClassInfo::mark();
}
c_R939::c_R939(){
}
c_R939* c_R939::m_new(){
	c_ClassInfo::m_new(String(L"monkey.map.NodeEnumerator<Int,monkey.lang.Object>",49),0,bb_reflection__classes[0],Array<c_ClassInfo* >());
	return this;
}
int c_R939::p_Init3(){
	gc_assign(m__fields,Array<c_FieldInfo* >(1));
	gc_assign(m__fields[0],((new c_R943)->m_new()));
	gc_assign(m__methods,Array<c_MethodInfo* >(2));
	gc_assign(m__methods[0],((new c_R941)->m_new()));
	gc_assign(m__methods[1],((new c_R942)->m_new()));
	gc_assign(m__ctors,Array<c_FunctionInfo* >(2));
	gc_assign(m__ctors[0],((new c_R940)->m_new()));
	gc_assign(m__ctors[1],((new c_R944)->m_new()));
	p_InitR();
	return 0;
}
void c_R939::mark(){
	c_ClassInfo::mark();
}
c_R945::c_R945(){
}
c_R945* c_R945::m_new(){
	c_ClassInfo::m_new(String(L"monkey.map.Node<Float,monkey.lang.Object>",41),0,bb_reflection__classes[0],Array<c_ClassInfo* >());
	return this;
}
int c_R945::p_Init3(){
	gc_assign(m__fields,Array<c_FieldInfo* >(6));
	gc_assign(m__fields[0],((new c_R953)->m_new()));
	gc_assign(m__fields[1],((new c_R954)->m_new()));
	gc_assign(m__fields[2],((new c_R955)->m_new()));
	gc_assign(m__fields[3],((new c_R956)->m_new()));
	gc_assign(m__fields[4],((new c_R957)->m_new()));
	gc_assign(m__fields[5],((new c_R958)->m_new()));
	gc_assign(m__methods,Array<c_MethodInfo* >(6));
	gc_assign(m__methods[0],((new c_R947)->m_new()));
	gc_assign(m__methods[1],((new c_R948)->m_new()));
	gc_assign(m__methods[2],((new c_R949)->m_new()));
	gc_assign(m__methods[3],((new c_R950)->m_new()));
	gc_assign(m__methods[4],((new c_R951)->m_new()));
	gc_assign(m__methods[5],((new c_R952)->m_new()));
	gc_assign(m__ctors,Array<c_FunctionInfo* >(2));
	gc_assign(m__ctors[0],((new c_R946)->m_new()));
	gc_assign(m__ctors[1],((new c_R959)->m_new()));
	p_InitR();
	return 0;
}
void c_R945::mark(){
	c_ClassInfo::mark();
}
c_R960::c_R960(){
}
c_R960* c_R960::m_new(){
	c_ClassInfo::m_new(String(L"monkey.map.MapKeys<Float,monkey.lang.Object>",44),0,bb_reflection__classes[0],Array<c_ClassInfo* >());
	return this;
}
int c_R960::p_Init3(){
	gc_assign(m__fields,Array<c_FieldInfo* >(1));
	gc_assign(m__fields[0],((new c_R963)->m_new()));
	gc_assign(m__methods,Array<c_MethodInfo* >(1));
	gc_assign(m__methods[0],((new c_R962)->m_new()));
	gc_assign(m__ctors,Array<c_FunctionInfo* >(2));
	gc_assign(m__ctors[0],((new c_R961)->m_new()));
	gc_assign(m__ctors[1],((new c_R964)->m_new()));
	p_InitR();
	return 0;
}
void c_R960::mark(){
	c_ClassInfo::mark();
}
c_R965::c_R965(){
}
c_R965* c_R965::m_new(){
	c_ClassInfo::m_new(String(L"monkey.map.MapValues<Float,monkey.lang.Object>",46),0,bb_reflection__classes[0],Array<c_ClassInfo* >());
	return this;
}
int c_R965::p_Init3(){
	gc_assign(m__fields,Array<c_FieldInfo* >(1));
	gc_assign(m__fields[0],((new c_R968)->m_new()));
	gc_assign(m__methods,Array<c_MethodInfo* >(1));
	gc_assign(m__methods[0],((new c_R967)->m_new()));
	gc_assign(m__ctors,Array<c_FunctionInfo* >(2));
	gc_assign(m__ctors[0],((new c_R966)->m_new()));
	gc_assign(m__ctors[1],((new c_R969)->m_new()));
	p_InitR();
	return 0;
}
void c_R965::mark(){
	c_ClassInfo::mark();
}
c_R970::c_R970(){
}
c_R970* c_R970::m_new(){
	c_ClassInfo::m_new(String(L"monkey.map.NodeEnumerator<Float,monkey.lang.Object>",51),0,bb_reflection__classes[0],Array<c_ClassInfo* >());
	return this;
}
int c_R970::p_Init3(){
	gc_assign(m__fields,Array<c_FieldInfo* >(1));
	gc_assign(m__fields[0],((new c_R974)->m_new()));
	gc_assign(m__methods,Array<c_MethodInfo* >(2));
	gc_assign(m__methods[0],((new c_R972)->m_new()));
	gc_assign(m__methods[1],((new c_R973)->m_new()));
	gc_assign(m__ctors,Array<c_FunctionInfo* >(2));
	gc_assign(m__ctors[0],((new c_R971)->m_new()));
	gc_assign(m__ctors[1],((new c_R975)->m_new()));
	p_InitR();
	return 0;
}
void c_R970::mark(){
	c_ClassInfo::mark();
}
c_R976::c_R976(){
}
c_R976* c_R976::m_new(){
	c_ClassInfo::m_new(String(L"monkey.map.Node<String,monkey.lang.Object>",42),0,bb_reflection__classes[0],Array<c_ClassInfo* >());
	return this;
}
int c_R976::p_Init3(){
	gc_assign(m__fields,Array<c_FieldInfo* >(6));
	gc_assign(m__fields[0],((new c_R984)->m_new()));
	gc_assign(m__fields[1],((new c_R985)->m_new()));
	gc_assign(m__fields[2],((new c_R986)->m_new()));
	gc_assign(m__fields[3],((new c_R987)->m_new()));
	gc_assign(m__fields[4],((new c_R988)->m_new()));
	gc_assign(m__fields[5],((new c_R989)->m_new()));
	gc_assign(m__methods,Array<c_MethodInfo* >(6));
	gc_assign(m__methods[0],((new c_R978)->m_new()));
	gc_assign(m__methods[1],((new c_R979)->m_new()));
	gc_assign(m__methods[2],((new c_R980)->m_new()));
	gc_assign(m__methods[3],((new c_R981)->m_new()));
	gc_assign(m__methods[4],((new c_R982)->m_new()));
	gc_assign(m__methods[5],((new c_R983)->m_new()));
	gc_assign(m__ctors,Array<c_FunctionInfo* >(2));
	gc_assign(m__ctors[0],((new c_R977)->m_new()));
	gc_assign(m__ctors[1],((new c_R990)->m_new()));
	p_InitR();
	return 0;
}
void c_R976::mark(){
	c_ClassInfo::mark();
}
c_R991::c_R991(){
}
c_R991* c_R991::m_new(){
	c_ClassInfo::m_new(String(L"monkey.map.MapKeys<String,monkey.lang.Object>",45),0,bb_reflection__classes[0],Array<c_ClassInfo* >());
	return this;
}
int c_R991::p_Init3(){
	gc_assign(m__fields,Array<c_FieldInfo* >(1));
	gc_assign(m__fields[0],((new c_R994)->m_new()));
	gc_assign(m__methods,Array<c_MethodInfo* >(1));
	gc_assign(m__methods[0],((new c_R993)->m_new()));
	gc_assign(m__ctors,Array<c_FunctionInfo* >(2));
	gc_assign(m__ctors[0],((new c_R992)->m_new()));
	gc_assign(m__ctors[1],((new c_R995)->m_new()));
	p_InitR();
	return 0;
}
void c_R991::mark(){
	c_ClassInfo::mark();
}
c_R996::c_R996(){
}
c_R996* c_R996::m_new(){
	c_ClassInfo::m_new(String(L"monkey.map.MapValues<String,monkey.lang.Object>",47),0,bb_reflection__classes[0],Array<c_ClassInfo* >());
	return this;
}
int c_R996::p_Init3(){
	gc_assign(m__fields,Array<c_FieldInfo* >(1));
	gc_assign(m__fields[0],((new c_R999)->m_new()));
	gc_assign(m__methods,Array<c_MethodInfo* >(1));
	gc_assign(m__methods[0],((new c_R998)->m_new()));
	gc_assign(m__ctors,Array<c_FunctionInfo* >(2));
	gc_assign(m__ctors[0],((new c_R997)->m_new()));
	gc_assign(m__ctors[1],((new c_R1000)->m_new()));
	p_InitR();
	return 0;
}
void c_R996::mark(){
	c_ClassInfo::mark();
}
c_R1001::c_R1001(){
}
c_R1001* c_R1001::m_new(){
	c_ClassInfo::m_new(String(L"monkey.map.NodeEnumerator<String,monkey.lang.Object>",52),0,bb_reflection__classes[0],Array<c_ClassInfo* >());
	return this;
}
int c_R1001::p_Init3(){
	gc_assign(m__fields,Array<c_FieldInfo* >(1));
	gc_assign(m__fields[0],((new c_R1005)->m_new()));
	gc_assign(m__methods,Array<c_MethodInfo* >(2));
	gc_assign(m__methods[0],((new c_R1003)->m_new()));
	gc_assign(m__methods[1],((new c_R1004)->m_new()));
	gc_assign(m__ctors,Array<c_FunctionInfo* >(2));
	gc_assign(m__ctors[0],((new c_R1002)->m_new()));
	gc_assign(m__ctors[1],((new c_R1006)->m_new()));
	p_InitR();
	return 0;
}
void c_R1001::mark(){
	c_ClassInfo::mark();
}
c_R1007::c_R1007(){
}
c_R1007* c_R1007::m_new(){
	c_ClassInfo::m_new(String(L"monkey.map.MapValues<String,String>",35),0,bb_reflection__classes[0],Array<c_ClassInfo* >());
	return this;
}
int c_R1007::p_Init3(){
	gc_assign(m__fields,Array<c_FieldInfo* >(1));
	gc_assign(m__fields[0],((new c_R1010)->m_new()));
	gc_assign(m__methods,Array<c_MethodInfo* >(1));
	gc_assign(m__methods[0],((new c_R1009)->m_new()));
	gc_assign(m__ctors,Array<c_FunctionInfo* >(2));
	gc_assign(m__ctors[0],((new c_R1008)->m_new()));
	gc_assign(m__ctors[1],((new c_R1011)->m_new()));
	p_InitR();
	return 0;
}
void c_R1007::mark(){
	c_ClassInfo::mark();
}
c_R1012::c_R1012(){
}
c_R1012* c_R1012::m_new(){
	c_ClassInfo::m_new(String(L"monkey.map.NodeEnumerator<String,String>",40),0,bb_reflection__classes[0],Array<c_ClassInfo* >());
	return this;
}
int c_R1012::p_Init3(){
	gc_assign(m__fields,Array<c_FieldInfo* >(1));
	gc_assign(m__fields[0],((new c_R1016)->m_new()));
	gc_assign(m__methods,Array<c_MethodInfo* >(2));
	gc_assign(m__methods[0],((new c_R1014)->m_new()));
	gc_assign(m__methods[1],((new c_R1015)->m_new()));
	gc_assign(m__ctors,Array<c_FunctionInfo* >(2));
	gc_assign(m__ctors[0],((new c_R1013)->m_new()));
	gc_assign(m__ctors[1],((new c_R1017)->m_new()));
	p_InitR();
	return 0;
}
void c_R1012::mark(){
	c_ClassInfo::mark();
}
c_R1018::c_R1018(){
}
c_R1018* c_R1018::m_new(){
	c_ClassInfo::m_new(String(L"monkey.map.KeyEnumerator<Int,monkey.lang.Object>",48),0,bb_reflection__classes[0],Array<c_ClassInfo* >());
	return this;
}
int c_R1018::p_Init3(){
	gc_assign(m__fields,Array<c_FieldInfo* >(1));
	gc_assign(m__fields[0],((new c_R1022)->m_new()));
	gc_assign(m__methods,Array<c_MethodInfo* >(2));
	gc_assign(m__methods[0],((new c_R1020)->m_new()));
	gc_assign(m__methods[1],((new c_R1021)->m_new()));
	gc_assign(m__ctors,Array<c_FunctionInfo* >(2));
	gc_assign(m__ctors[0],((new c_R1019)->m_new()));
	gc_assign(m__ctors[1],((new c_R1023)->m_new()));
	p_InitR();
	return 0;
}
void c_R1018::mark(){
	c_ClassInfo::mark();
}
c_R1024::c_R1024(){
}
c_R1024* c_R1024::m_new(){
	c_ClassInfo::m_new(String(L"monkey.map.KeyEnumerator<Float,monkey.lang.Object>",50),0,bb_reflection__classes[0],Array<c_ClassInfo* >());
	return this;
}
int c_R1024::p_Init3(){
	gc_assign(m__fields,Array<c_FieldInfo* >(1));
	gc_assign(m__fields[0],((new c_R1028)->m_new()));
	gc_assign(m__methods,Array<c_MethodInfo* >(2));
	gc_assign(m__methods[0],((new c_R1026)->m_new()));
	gc_assign(m__methods[1],((new c_R1027)->m_new()));
	gc_assign(m__ctors,Array<c_FunctionInfo* >(2));
	gc_assign(m__ctors[0],((new c_R1025)->m_new()));
	gc_assign(m__ctors[1],((new c_R1029)->m_new()));
	p_InitR();
	return 0;
}
void c_R1024::mark(){
	c_ClassInfo::mark();
}
c_R1030::c_R1030(){
}
c_R1030* c_R1030::m_new(){
	c_ClassInfo::m_new(String(L"monkey.map.KeyEnumerator<String,monkey.lang.Object>",51),0,bb_reflection__classes[0],Array<c_ClassInfo* >());
	return this;
}
int c_R1030::p_Init3(){
	gc_assign(m__fields,Array<c_FieldInfo* >(1));
	gc_assign(m__fields[0],((new c_R1034)->m_new()));
	gc_assign(m__methods,Array<c_MethodInfo* >(2));
	gc_assign(m__methods[0],((new c_R1032)->m_new()));
	gc_assign(m__methods[1],((new c_R1033)->m_new()));
	gc_assign(m__ctors,Array<c_FunctionInfo* >(2));
	gc_assign(m__ctors[0],((new c_R1031)->m_new()));
	gc_assign(m__ctors[1],((new c_R1035)->m_new()));
	p_InitR();
	return 0;
}
void c_R1030::mark(){
	c_ClassInfo::mark();
}
c_R1036::c_R1036(){
}
c_R1036* c_R1036::m_new(){
	c_ClassInfo::m_new(String(L"monkey.map.ValueEnumerator<Int,monkey.lang.Object>",50),0,bb_reflection__classes[0],Array<c_ClassInfo* >());
	return this;
}
int c_R1036::p_Init3(){
	gc_assign(m__fields,Array<c_FieldInfo* >(1));
	gc_assign(m__fields[0],((new c_R1040)->m_new()));
	gc_assign(m__methods,Array<c_MethodInfo* >(2));
	gc_assign(m__methods[0],((new c_R1038)->m_new()));
	gc_assign(m__methods[1],((new c_R1039)->m_new()));
	gc_assign(m__ctors,Array<c_FunctionInfo* >(2));
	gc_assign(m__ctors[0],((new c_R1037)->m_new()));
	gc_assign(m__ctors[1],((new c_R1041)->m_new()));
	p_InitR();
	return 0;
}
void c_R1036::mark(){
	c_ClassInfo::mark();
}
c_R1042::c_R1042(){
}
c_R1042* c_R1042::m_new(){
	c_ClassInfo::m_new(String(L"monkey.map.ValueEnumerator<Float,monkey.lang.Object>",52),0,bb_reflection__classes[0],Array<c_ClassInfo* >());
	return this;
}
int c_R1042::p_Init3(){
	gc_assign(m__fields,Array<c_FieldInfo* >(1));
	gc_assign(m__fields[0],((new c_R1046)->m_new()));
	gc_assign(m__methods,Array<c_MethodInfo* >(2));
	gc_assign(m__methods[0],((new c_R1044)->m_new()));
	gc_assign(m__methods[1],((new c_R1045)->m_new()));
	gc_assign(m__ctors,Array<c_FunctionInfo* >(2));
	gc_assign(m__ctors[0],((new c_R1043)->m_new()));
	gc_assign(m__ctors[1],((new c_R1047)->m_new()));
	p_InitR();
	return 0;
}
void c_R1042::mark(){
	c_ClassInfo::mark();
}
c_R1048::c_R1048(){
}
c_R1048* c_R1048::m_new(){
	c_ClassInfo::m_new(String(L"monkey.map.ValueEnumerator<String,monkey.lang.Object>",53),0,bb_reflection__classes[0],Array<c_ClassInfo* >());
	return this;
}
int c_R1048::p_Init3(){
	gc_assign(m__fields,Array<c_FieldInfo* >(1));
	gc_assign(m__fields[0],((new c_R1052)->m_new()));
	gc_assign(m__methods,Array<c_MethodInfo* >(2));
	gc_assign(m__methods[0],((new c_R1050)->m_new()));
	gc_assign(m__methods[1],((new c_R1051)->m_new()));
	gc_assign(m__ctors,Array<c_FunctionInfo* >(2));
	gc_assign(m__ctors[0],((new c_R1049)->m_new()));
	gc_assign(m__ctors[1],((new c_R1053)->m_new()));
	p_InitR();
	return 0;
}
void c_R1048::mark(){
	c_ClassInfo::mark();
}
c_R1054::c_R1054(){
}
c_R1054* c_R1054::m_new(){
	c_ClassInfo::m_new(String(L"monkey.map.ValueEnumerator<String,String>",41),0,bb_reflection__classes[0],Array<c_ClassInfo* >());
	return this;
}
int c_R1054::p_Init3(){
	gc_assign(m__fields,Array<c_FieldInfo* >(1));
	gc_assign(m__fields[0],((new c_R1058)->m_new()));
	gc_assign(m__methods,Array<c_MethodInfo* >(2));
	gc_assign(m__methods[0],((new c_R1056)->m_new()));
	gc_assign(m__methods[1],((new c_R1057)->m_new()));
	gc_assign(m__ctors,Array<c_FunctionInfo* >(2));
	gc_assign(m__ctors[0],((new c_R1055)->m_new()));
	gc_assign(m__ctors[1],((new c_R1059)->m_new()));
	p_InitR();
	return 0;
}
void c_R1054::mark(){
	c_ClassInfo::mark();
}
c_R1060::c_R1060(){
}
c_R1060* c_R1060::m_new(){
	c_ClassInfo::m_new(String(L"monkey.stack.Enumerator<Int>",28),0,bb_reflection__classes[0],Array<c_ClassInfo* >());
	return this;
}
int c_R1060::p_Init3(){
	gc_assign(m__fields,Array<c_FieldInfo* >(2));
	gc_assign(m__fields[0],((new c_R1064)->m_new()));
	gc_assign(m__fields[1],((new c_R1065)->m_new()));
	gc_assign(m__methods,Array<c_MethodInfo* >(2));
	gc_assign(m__methods[0],((new c_R1062)->m_new()));
	gc_assign(m__methods[1],((new c_R1063)->m_new()));
	gc_assign(m__ctors,Array<c_FunctionInfo* >(2));
	gc_assign(m__ctors[0],((new c_R1061)->m_new()));
	gc_assign(m__ctors[1],((new c_R1066)->m_new()));
	p_InitR();
	return 0;
}
void c_R1060::mark(){
	c_ClassInfo::mark();
}
c_R1067::c_R1067(){
}
c_R1067* c_R1067::m_new(){
	c_ClassInfo::m_new(String(L"monkey.stack.BackwardsStack<Int>",32),0,bb_reflection__classes[0],Array<c_ClassInfo* >());
	return this;
}
int c_R1067::p_Init3(){
	gc_assign(m__fields,Array<c_FieldInfo* >(1));
	gc_assign(m__fields[0],((new c_R1070)->m_new()));
	gc_assign(m__methods,Array<c_MethodInfo* >(1));
	gc_assign(m__methods[0],((new c_R1069)->m_new()));
	gc_assign(m__ctors,Array<c_FunctionInfo* >(2));
	gc_assign(m__ctors[0],((new c_R1068)->m_new()));
	gc_assign(m__ctors[1],((new c_R1071)->m_new()));
	p_InitR();
	return 0;
}
void c_R1067::mark(){
	c_ClassInfo::mark();
}
c_R1072::c_R1072(){
}
c_R1072* c_R1072::m_new(){
	c_ClassInfo::m_new(String(L"monkey.stack.Enumerator<Float>",30),0,bb_reflection__classes[0],Array<c_ClassInfo* >());
	return this;
}
int c_R1072::p_Init3(){
	gc_assign(m__fields,Array<c_FieldInfo* >(2));
	gc_assign(m__fields[0],((new c_R1076)->m_new()));
	gc_assign(m__fields[1],((new c_R1077)->m_new()));
	gc_assign(m__methods,Array<c_MethodInfo* >(2));
	gc_assign(m__methods[0],((new c_R1074)->m_new()));
	gc_assign(m__methods[1],((new c_R1075)->m_new()));
	gc_assign(m__ctors,Array<c_FunctionInfo* >(2));
	gc_assign(m__ctors[0],((new c_R1073)->m_new()));
	gc_assign(m__ctors[1],((new c_R1078)->m_new()));
	p_InitR();
	return 0;
}
void c_R1072::mark(){
	c_ClassInfo::mark();
}
c_R1079::c_R1079(){
}
c_R1079* c_R1079::m_new(){
	c_ClassInfo::m_new(String(L"monkey.stack.BackwardsStack<Float>",34),0,bb_reflection__classes[0],Array<c_ClassInfo* >());
	return this;
}
int c_R1079::p_Init3(){
	gc_assign(m__fields,Array<c_FieldInfo* >(1));
	gc_assign(m__fields[0],((new c_R1082)->m_new()));
	gc_assign(m__methods,Array<c_MethodInfo* >(1));
	gc_assign(m__methods[0],((new c_R1081)->m_new()));
	gc_assign(m__ctors,Array<c_FunctionInfo* >(2));
	gc_assign(m__ctors[0],((new c_R1080)->m_new()));
	gc_assign(m__ctors[1],((new c_R1083)->m_new()));
	p_InitR();
	return 0;
}
void c_R1079::mark(){
	c_ClassInfo::mark();
}
c_R1084::c_R1084(){
}
c_R1084* c_R1084::m_new(){
	c_ClassInfo::m_new(String(L"monkey.stack.Enumerator<String>",31),0,bb_reflection__classes[0],Array<c_ClassInfo* >());
	return this;
}
int c_R1084::p_Init3(){
	gc_assign(m__fields,Array<c_FieldInfo* >(2));
	gc_assign(m__fields[0],((new c_R1088)->m_new()));
	gc_assign(m__fields[1],((new c_R1089)->m_new()));
	gc_assign(m__methods,Array<c_MethodInfo* >(2));
	gc_assign(m__methods[0],((new c_R1086)->m_new()));
	gc_assign(m__methods[1],((new c_R1087)->m_new()));
	gc_assign(m__ctors,Array<c_FunctionInfo* >(2));
	gc_assign(m__ctors[0],((new c_R1085)->m_new()));
	gc_assign(m__ctors[1],((new c_R1090)->m_new()));
	p_InitR();
	return 0;
}
void c_R1084::mark(){
	c_ClassInfo::mark();
}
c_R1091::c_R1091(){
}
c_R1091* c_R1091::m_new(){
	c_ClassInfo::m_new(String(L"monkey.stack.BackwardsStack<String>",35),0,bb_reflection__classes[0],Array<c_ClassInfo* >());
	return this;
}
int c_R1091::p_Init3(){
	gc_assign(m__fields,Array<c_FieldInfo* >(1));
	gc_assign(m__fields[0],((new c_R1094)->m_new()));
	gc_assign(m__methods,Array<c_MethodInfo* >(1));
	gc_assign(m__methods[0],((new c_R1093)->m_new()));
	gc_assign(m__ctors,Array<c_FunctionInfo* >(2));
	gc_assign(m__ctors[0],((new c_R1092)->m_new()));
	gc_assign(m__ctors[1],((new c_R1095)->m_new()));
	p_InitR();
	return 0;
}
void c_R1091::mark(){
	c_ClassInfo::mark();
}
c_R1096::c_R1096(){
}
c_R1096* c_R1096::m_new(){
	c_ClassInfo::m_new(String(L"monkey.stack.BackwardsEnumerator<Int>",37),0,bb_reflection__classes[0],Array<c_ClassInfo* >());
	return this;
}
int c_R1096::p_Init3(){
	gc_assign(m__fields,Array<c_FieldInfo* >(2));
	gc_assign(m__fields[0],((new c_R1100)->m_new()));
	gc_assign(m__fields[1],((new c_R1101)->m_new()));
	gc_assign(m__methods,Array<c_MethodInfo* >(2));
	gc_assign(m__methods[0],((new c_R1098)->m_new()));
	gc_assign(m__methods[1],((new c_R1099)->m_new()));
	gc_assign(m__ctors,Array<c_FunctionInfo* >(2));
	gc_assign(m__ctors[0],((new c_R1097)->m_new()));
	gc_assign(m__ctors[1],((new c_R1102)->m_new()));
	p_InitR();
	return 0;
}
void c_R1096::mark(){
	c_ClassInfo::mark();
}
c_R1103::c_R1103(){
}
c_R1103* c_R1103::m_new(){
	c_ClassInfo::m_new(String(L"monkey.stack.BackwardsEnumerator<Float>",39),0,bb_reflection__classes[0],Array<c_ClassInfo* >());
	return this;
}
int c_R1103::p_Init3(){
	gc_assign(m__fields,Array<c_FieldInfo* >(2));
	gc_assign(m__fields[0],((new c_R1107)->m_new()));
	gc_assign(m__fields[1],((new c_R1108)->m_new()));
	gc_assign(m__methods,Array<c_MethodInfo* >(2));
	gc_assign(m__methods[0],((new c_R1105)->m_new()));
	gc_assign(m__methods[1],((new c_R1106)->m_new()));
	gc_assign(m__ctors,Array<c_FunctionInfo* >(2));
	gc_assign(m__ctors[0],((new c_R1104)->m_new()));
	gc_assign(m__ctors[1],((new c_R1109)->m_new()));
	p_InitR();
	return 0;
}
void c_R1103::mark(){
	c_ClassInfo::mark();
}
c_R1110::c_R1110(){
}
c_R1110* c_R1110::m_new(){
	c_ClassInfo::m_new(String(L"monkey.stack.BackwardsEnumerator<String>",40),0,bb_reflection__classes[0],Array<c_ClassInfo* >());
	return this;
}
int c_R1110::p_Init3(){
	gc_assign(m__fields,Array<c_FieldInfo* >(2));
	gc_assign(m__fields[0],((new c_R1114)->m_new()));
	gc_assign(m__fields[1],((new c_R1115)->m_new()));
	gc_assign(m__methods,Array<c_MethodInfo* >(2));
	gc_assign(m__methods[0],((new c_R1112)->m_new()));
	gc_assign(m__methods[1],((new c_R1113)->m_new()));
	gc_assign(m__ctors,Array<c_FunctionInfo* >(2));
	gc_assign(m__ctors[0],((new c_R1111)->m_new()));
	gc_assign(m__ctors[1],((new c_R1116)->m_new()));
	p_InitR();
	return 0;
}
void c_R1110::mark(){
	c_ClassInfo::mark();
}
c_R1117::c_R1117(){
}
c_R1117* c_R1117::m_new(){
	c_ClassInfo::m_new(String(L"monkey.boxes.ArrayObject<Int>",29),0,bb_reflection__classes[0],Array<c_ClassInfo* >());
	return this;
}
int c_R1117::p_Init3(){
	gc_assign(m__fields,Array<c_FieldInfo* >(1));
	gc_assign(m__fields[0],((new c_R1118)->m_new()));
	gc_assign(m__methods,Array<c_MethodInfo* >(1));
	gc_assign(m__methods[0],((new c_R1120)->m_new()));
	gc_assign(m__ctors,Array<c_FunctionInfo* >(2));
	gc_assign(m__ctors[0],((new c_R1119)->m_new()));
	gc_assign(m__ctors[1],((new c_R1121)->m_new()));
	p_InitR();
	return 0;
}
void c_R1117::mark(){
	c_ClassInfo::mark();
}
c_R1122::c_R1122(){
}
c_R1122* c_R1122::m_new(){
	c_ClassInfo::m_new(String(L"monkey.boxes.ArrayObject<Float>",31),0,bb_reflection__classes[0],Array<c_ClassInfo* >());
	return this;
}
int c_R1122::p_Init3(){
	gc_assign(m__fields,Array<c_FieldInfo* >(1));
	gc_assign(m__fields[0],((new c_R1123)->m_new()));
	gc_assign(m__methods,Array<c_MethodInfo* >(1));
	gc_assign(m__methods[0],((new c_R1125)->m_new()));
	gc_assign(m__ctors,Array<c_FunctionInfo* >(2));
	gc_assign(m__ctors[0],((new c_R1124)->m_new()));
	gc_assign(m__ctors[1],((new c_R1126)->m_new()));
	p_InitR();
	return 0;
}
void c_R1122::mark(){
	c_ClassInfo::mark();
}
c_R1127::c_R1127(){
}
c_R1127* c_R1127::m_new(){
	c_ClassInfo::m_new(String(L"monkey.boxes.ArrayObject<String>",32),0,bb_reflection__classes[0],Array<c_ClassInfo* >());
	return this;
}
int c_R1127::p_Init3(){
	gc_assign(m__fields,Array<c_FieldInfo* >(1));
	gc_assign(m__fields[0],((new c_R1128)->m_new()));
	gc_assign(m__methods,Array<c_MethodInfo* >(1));
	gc_assign(m__methods[0],((new c_R1130)->m_new()));
	gc_assign(m__ctors,Array<c_FunctionInfo* >(2));
	gc_assign(m__ctors[0],((new c_R1129)->m_new()));
	gc_assign(m__ctors[1],((new c_R1131)->m_new()));
	p_InitR();
	return 0;
}
void c_R1127::mark(){
	c_ClassInfo::mark();
}
c_ConstInfo::c_ConstInfo(){
	m__name=String();
	m__attrs=0;
	m__type=0;
	m__value=0;
}
c_ConstInfo* c_ConstInfo::m_new(String t_name,int t_attrs,c_ClassInfo* t_type,Object* t_value){
	m__name=t_name;
	m__attrs=t_attrs;
	gc_assign(m__type,t_type);
	gc_assign(m__value,t_value);
	return this;
}
c_ConstInfo* c_ConstInfo::m_new2(){
	return this;
}
void c_ConstInfo::mark(){
	Object::mark();
	gc_mark_q(m__type);
	gc_mark_q(m__value);
}
Array<c_ConstInfo* > bb_reflection__consts;
c_GlobalInfo::c_GlobalInfo(){
	m__name=String();
	m__attrs=0;
	m__type=0;
}
c_GlobalInfo* c_GlobalInfo::m_new(String t_name,int t_attrs,c_ClassInfo* t_type){
	m__name=t_name;
	m__attrs=t_attrs;
	gc_assign(m__type,t_type);
	return this;
}
c_GlobalInfo* c_GlobalInfo::m_new2(){
	return this;
}
void c_GlobalInfo::mark(){
	Object::mark();
	gc_mark_q(m__type);
}
Array<c_GlobalInfo* > bb_reflection__globals;
c_R59::c_R59(){
}
c_R59* c_R59::m_new(){
	c_GlobalInfo::m_new(String(L"monkey.random.Seed",18),0,bb_reflection__intClass);
	return this;
}
void c_R59::mark(){
	c_GlobalInfo::mark();
}
c_FunctionInfo::c_FunctionInfo(){
	m__name=String();
	m__attrs=0;
	m__retType=0;
	m__argTypes=Array<c_ClassInfo* >();
}
c_FunctionInfo* c_FunctionInfo::m_new(String t_name,int t_attrs,c_ClassInfo* t_retType,Array<c_ClassInfo* > t_argTypes){
	m__name=t_name;
	m__attrs=t_attrs;
	gc_assign(m__retType,t_retType);
	gc_assign(m__argTypes,t_argTypes);
	return this;
}
c_FunctionInfo* c_FunctionInfo::m_new2(){
	return this;
}
void c_FunctionInfo::mark(){
	Object::mark();
	gc_mark_q(m__retType);
	gc_mark_q(m__argTypes);
}
Array<c_FunctionInfo* > bb_reflection__functions;
c_R17::c_R17(){
}
c_R17* c_R17::m_new(){
	c_ClassInfo* t_[]={bb_reflection__boolClass};
	c_FunctionInfo::m_new(String(L"monkey.boxes.BoxBool",20),0,bb_reflection__classes[0],Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R17::mark(){
	c_FunctionInfo::mark();
}
c_R18::c_R18(){
}
c_R18* c_R18::m_new(){
	c_ClassInfo* t_[]={bb_reflection__intClass};
	c_FunctionInfo::m_new(String(L"monkey.boxes.BoxInt",19),0,bb_reflection__classes[0],Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R18::mark(){
	c_FunctionInfo::mark();
}
c_R19::c_R19(){
}
c_R19* c_R19::m_new(){
	c_ClassInfo* t_[]={bb_reflection__floatClass};
	c_FunctionInfo::m_new(String(L"monkey.boxes.BoxFloat",21),0,bb_reflection__classes[0],Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R19::mark(){
	c_FunctionInfo::mark();
}
c_R20::c_R20(){
}
c_R20* c_R20::m_new(){
	c_ClassInfo* t_[]={bb_reflection__stringClass};
	c_FunctionInfo::m_new(String(L"monkey.boxes.BoxString",22),0,bb_reflection__classes[0],Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R20::mark(){
	c_FunctionInfo::mark();
}
c_R21::c_R21(){
}
c_R21* c_R21::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[0]};
	c_FunctionInfo::m_new(String(L"monkey.boxes.UnboxBool",22),0,bb_reflection__boolClass,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R21::mark(){
	c_FunctionInfo::mark();
}
c_R22::c_R22(){
}
c_R22* c_R22::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[0]};
	c_FunctionInfo::m_new(String(L"monkey.boxes.UnboxInt",21),0,bb_reflection__intClass,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R22::mark(){
	c_FunctionInfo::mark();
}
c_R23::c_R23(){
}
c_R23* c_R23::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[0]};
	c_FunctionInfo::m_new(String(L"monkey.boxes.UnboxFloat",23),0,bb_reflection__floatClass,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R23::mark(){
	c_FunctionInfo::mark();
}
c_R24::c_R24(){
}
c_R24* c_R24::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[0]};
	c_FunctionInfo::m_new(String(L"monkey.boxes.UnboxString",24),0,bb_reflection__stringClass,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R24::mark(){
	c_FunctionInfo::mark();
}
c_R25::c_R25(){
}
c_R25* c_R25::m_new(){
	c_ClassInfo* t_[]={bb_reflection__stringClass};
	c_FunctionInfo::m_new(String(L"monkey.lang.Print",17),1,bb_reflection__intClass,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R25::mark(){
	c_FunctionInfo::mark();
}
c_R26::c_R26(){
}
c_R26* c_R26::m_new(){
	c_ClassInfo* t_[]={bb_reflection__stringClass};
	c_FunctionInfo::m_new(String(L"monkey.lang.Error",17),1,bb_reflection__intClass,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R26::mark(){
	c_FunctionInfo::mark();
}
c_R27::c_R27(){
}
c_R27* c_R27::m_new(){
	c_ClassInfo* t_[]={bb_reflection__stringClass};
	c_FunctionInfo::m_new(String(L"monkey.lang.DebugLog",20),1,bb_reflection__intClass,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R27::mark(){
	c_FunctionInfo::mark();
}
c_R28::c_R28(){
}
c_R28* c_R28::m_new(){
	c_FunctionInfo::m_new(String(L"monkey.lang.DebugStop",21),1,bb_reflection__intClass,Array<c_ClassInfo* >());
	return this;
}
void c_R28::mark(){
	c_FunctionInfo::mark();
}
c_R29::c_R29(){
}
c_R29* c_R29::m_new(){
	c_ClassInfo* t_[]={bb_reflection__floatClass};
	c_FunctionInfo::m_new(String(L"monkey.math.Sin",15),1,bb_reflection__floatClass,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R29::mark(){
	c_FunctionInfo::mark();
}
c_R30::c_R30(){
}
c_R30* c_R30::m_new(){
	c_ClassInfo* t_[]={bb_reflection__floatClass};
	c_FunctionInfo::m_new(String(L"monkey.math.Cos",15),1,bb_reflection__floatClass,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R30::mark(){
	c_FunctionInfo::mark();
}
c_R31::c_R31(){
}
c_R31* c_R31::m_new(){
	c_ClassInfo* t_[]={bb_reflection__floatClass};
	c_FunctionInfo::m_new(String(L"monkey.math.Tan",15),1,bb_reflection__floatClass,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R31::mark(){
	c_FunctionInfo::mark();
}
c_R32::c_R32(){
}
c_R32* c_R32::m_new(){
	c_ClassInfo* t_[]={bb_reflection__floatClass};
	c_FunctionInfo::m_new(String(L"monkey.math.ASin",16),1,bb_reflection__floatClass,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R32::mark(){
	c_FunctionInfo::mark();
}
c_R33::c_R33(){
}
c_R33* c_R33::m_new(){
	c_ClassInfo* t_[]={bb_reflection__floatClass};
	c_FunctionInfo::m_new(String(L"monkey.math.ACos",16),1,bb_reflection__floatClass,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R33::mark(){
	c_FunctionInfo::mark();
}
c_R34::c_R34(){
}
c_R34* c_R34::m_new(){
	c_ClassInfo* t_[]={bb_reflection__floatClass};
	c_FunctionInfo::m_new(String(L"monkey.math.ATan",16),1,bb_reflection__floatClass,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R34::mark(){
	c_FunctionInfo::mark();
}
c_R35::c_R35(){
}
c_R35* c_R35::m_new(){
	c_ClassInfo* t_[]={bb_reflection__floatClass,bb_reflection__floatClass};
	c_FunctionInfo::m_new(String(L"monkey.math.ATan2",17),1,bb_reflection__floatClass,Array<c_ClassInfo* >(t_,2));
	return this;
}
void c_R35::mark(){
	c_FunctionInfo::mark();
}
c_R36::c_R36(){
}
c_R36* c_R36::m_new(){
	c_ClassInfo* t_[]={bb_reflection__floatClass};
	c_FunctionInfo::m_new(String(L"monkey.math.Sinr",16),1,bb_reflection__floatClass,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R36::mark(){
	c_FunctionInfo::mark();
}
c_R37::c_R37(){
}
c_R37* c_R37::m_new(){
	c_ClassInfo* t_[]={bb_reflection__floatClass};
	c_FunctionInfo::m_new(String(L"monkey.math.Cosr",16),1,bb_reflection__floatClass,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R37::mark(){
	c_FunctionInfo::mark();
}
c_R38::c_R38(){
}
c_R38* c_R38::m_new(){
	c_ClassInfo* t_[]={bb_reflection__floatClass};
	c_FunctionInfo::m_new(String(L"monkey.math.Tanr",16),1,bb_reflection__floatClass,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R38::mark(){
	c_FunctionInfo::mark();
}
c_R39::c_R39(){
}
c_R39* c_R39::m_new(){
	c_ClassInfo* t_[]={bb_reflection__floatClass};
	c_FunctionInfo::m_new(String(L"monkey.math.ASinr",17),1,bb_reflection__floatClass,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R39::mark(){
	c_FunctionInfo::mark();
}
c_R40::c_R40(){
}
c_R40* c_R40::m_new(){
	c_ClassInfo* t_[]={bb_reflection__floatClass};
	c_FunctionInfo::m_new(String(L"monkey.math.ACosr",17),1,bb_reflection__floatClass,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R40::mark(){
	c_FunctionInfo::mark();
}
c_R41::c_R41(){
}
c_R41* c_R41::m_new(){
	c_ClassInfo* t_[]={bb_reflection__floatClass};
	c_FunctionInfo::m_new(String(L"monkey.math.ATanr",17),1,bb_reflection__floatClass,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R41::mark(){
	c_FunctionInfo::mark();
}
c_R42::c_R42(){
}
c_R42* c_R42::m_new(){
	c_ClassInfo* t_[]={bb_reflection__floatClass,bb_reflection__floatClass};
	c_FunctionInfo::m_new(String(L"monkey.math.ATan2r",18),1,bb_reflection__floatClass,Array<c_ClassInfo* >(t_,2));
	return this;
}
void c_R42::mark(){
	c_FunctionInfo::mark();
}
c_R43::c_R43(){
}
c_R43* c_R43::m_new(){
	c_ClassInfo* t_[]={bb_reflection__floatClass};
	c_FunctionInfo::m_new(String(L"monkey.math.Sqrt",16),1,bb_reflection__floatClass,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R43::mark(){
	c_FunctionInfo::mark();
}
c_R44::c_R44(){
}
c_R44* c_R44::m_new(){
	c_ClassInfo* t_[]={bb_reflection__floatClass};
	c_FunctionInfo::m_new(String(L"monkey.math.Floor",17),1,bb_reflection__floatClass,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R44::mark(){
	c_FunctionInfo::mark();
}
c_R45::c_R45(){
}
c_R45* c_R45::m_new(){
	c_ClassInfo* t_[]={bb_reflection__floatClass};
	c_FunctionInfo::m_new(String(L"monkey.math.Ceil",16),1,bb_reflection__floatClass,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R45::mark(){
	c_FunctionInfo::mark();
}
c_R46::c_R46(){
}
c_R46* c_R46::m_new(){
	c_ClassInfo* t_[]={bb_reflection__floatClass};
	c_FunctionInfo::m_new(String(L"monkey.math.Log",15),1,bb_reflection__floatClass,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R46::mark(){
	c_FunctionInfo::mark();
}
c_R47::c_R47(){
}
c_R47* c_R47::m_new(){
	c_ClassInfo* t_[]={bb_reflection__floatClass,bb_reflection__floatClass};
	c_FunctionInfo::m_new(String(L"monkey.math.Pow",15),1,bb_reflection__floatClass,Array<c_ClassInfo* >(t_,2));
	return this;
}
void c_R47::mark(){
	c_FunctionInfo::mark();
}
c_R48::c_R48(){
}
c_R48* c_R48::m_new(){
	c_ClassInfo* t_[]={bb_reflection__floatClass};
	c_FunctionInfo::m_new(String(L"monkey.math.Exp",15),1,bb_reflection__floatClass,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R48::mark(){
	c_FunctionInfo::mark();
}
c_R49::c_R49(){
}
c_R49* c_R49::m_new(){
	c_ClassInfo* t_[]={bb_reflection__intClass};
	c_FunctionInfo::m_new(String(L"monkey.math.Sgn",15),0,bb_reflection__intClass,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R49::mark(){
	c_FunctionInfo::mark();
}
c_R50::c_R50(){
}
c_R50* c_R50::m_new(){
	c_ClassInfo* t_[]={bb_reflection__intClass};
	c_FunctionInfo::m_new(String(L"monkey.math.Abs",15),0,bb_reflection__intClass,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R50::mark(){
	c_FunctionInfo::mark();
}
c_R51::c_R51(){
}
c_R51* c_R51::m_new(){
	c_ClassInfo* t_[]={bb_reflection__intClass,bb_reflection__intClass};
	c_FunctionInfo::m_new(String(L"monkey.math.Min",15),0,bb_reflection__intClass,Array<c_ClassInfo* >(t_,2));
	return this;
}
void c_R51::mark(){
	c_FunctionInfo::mark();
}
c_R52::c_R52(){
}
c_R52* c_R52::m_new(){
	c_ClassInfo* t_[]={bb_reflection__intClass,bb_reflection__intClass};
	c_FunctionInfo::m_new(String(L"monkey.math.Max",15),0,bb_reflection__intClass,Array<c_ClassInfo* >(t_,2));
	return this;
}
void c_R52::mark(){
	c_FunctionInfo::mark();
}
c_R53::c_R53(){
}
c_R53* c_R53::m_new(){
	c_ClassInfo* t_[]={bb_reflection__intClass,bb_reflection__intClass,bb_reflection__intClass};
	c_FunctionInfo::m_new(String(L"monkey.math.Clamp",17),0,bb_reflection__intClass,Array<c_ClassInfo* >(t_,3));
	return this;
}
void c_R53::mark(){
	c_FunctionInfo::mark();
}
c_R54::c_R54(){
}
c_R54* c_R54::m_new(){
	c_ClassInfo* t_[]={bb_reflection__floatClass};
	c_FunctionInfo::m_new(String(L"monkey.math.Sgn",15),0,bb_reflection__floatClass,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R54::mark(){
	c_FunctionInfo::mark();
}
c_R55::c_R55(){
}
c_R55* c_R55::m_new(){
	c_ClassInfo* t_[]={bb_reflection__floatClass};
	c_FunctionInfo::m_new(String(L"monkey.math.Abs",15),0,bb_reflection__floatClass,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R55::mark(){
	c_FunctionInfo::mark();
}
c_R56::c_R56(){
}
c_R56* c_R56::m_new(){
	c_ClassInfo* t_[]={bb_reflection__floatClass,bb_reflection__floatClass};
	c_FunctionInfo::m_new(String(L"monkey.math.Min",15),0,bb_reflection__floatClass,Array<c_ClassInfo* >(t_,2));
	return this;
}
void c_R56::mark(){
	c_FunctionInfo::mark();
}
c_R57::c_R57(){
}
c_R57* c_R57::m_new(){
	c_ClassInfo* t_[]={bb_reflection__floatClass,bb_reflection__floatClass};
	c_FunctionInfo::m_new(String(L"monkey.math.Max",15),0,bb_reflection__floatClass,Array<c_ClassInfo* >(t_,2));
	return this;
}
void c_R57::mark(){
	c_FunctionInfo::mark();
}
c_R58::c_R58(){
}
c_R58* c_R58::m_new(){
	c_ClassInfo* t_[]={bb_reflection__floatClass,bb_reflection__floatClass,bb_reflection__floatClass};
	c_FunctionInfo::m_new(String(L"monkey.math.Clamp",17),0,bb_reflection__floatClass,Array<c_ClassInfo* >(t_,3));
	return this;
}
void c_R58::mark(){
	c_FunctionInfo::mark();
}
c_R60::c_R60(){
}
c_R60* c_R60::m_new(){
	c_FunctionInfo::m_new(String(L"monkey.random.Rnd",17),0,bb_reflection__floatClass,Array<c_ClassInfo* >());
	return this;
}
void c_R60::mark(){
	c_FunctionInfo::mark();
}
c_R61::c_R61(){
}
c_R61* c_R61::m_new(){
	c_ClassInfo* t_[]={bb_reflection__floatClass,bb_reflection__floatClass};
	c_FunctionInfo::m_new(String(L"monkey.random.Rnd",17),0,bb_reflection__floatClass,Array<c_ClassInfo* >(t_,2));
	return this;
}
void c_R61::mark(){
	c_FunctionInfo::mark();
}
c_R62::c_R62(){
}
c_R62* c_R62::m_new(){
	c_ClassInfo* t_[]={bb_reflection__floatClass};
	c_FunctionInfo::m_new(String(L"monkey.random.Rnd",17),0,bb_reflection__floatClass,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R62::mark(){
	c_FunctionInfo::mark();
}
c__GetClass::c__GetClass(){
}
c__GetClass* c__GetClass::m_new(){
	return this;
}
void c__GetClass::mark(){
	Object::mark();
}
c___GetClass::c___GetClass(){
}
c___GetClass* c___GetClass::m_new(){
	c__GetClass::m_new();
	return this;
}
void c___GetClass::mark(){
	c__GetClass::mark();
}
c__GetClass* bb_reflection__getClass;
int bb_reflection___init(){
	gc_assign(bb_reflection__classes,Array<c_ClassInfo* >(101));
	gc_assign(bb_reflection__classes[0],((new c_R63)->m_new()));
	gc_assign(bb_reflection__classes[1],((new c_R64)->m_new()));
	gc_assign(bb_reflection__classes[2],((new c_R70)->m_new()));
	gc_assign(bb_reflection__classes[3],((new c_R80)->m_new()));
	gc_assign(bb_reflection__classes[4],((new c_R90)->m_new()));
	gc_assign(bb_reflection__classes[5],((new c_R99)->m_new()));
	gc_assign(bb_reflection__classes[6],((new c_R121)->m_new()));
	gc_assign(bb_reflection__classes[7],((new c_R124)->m_new()));
	gc_assign(bb_reflection__classes[8],((new c_R146)->m_new()));
	gc_assign(bb_reflection__classes[9],((new c_R149)->m_new()));
	gc_assign(bb_reflection__classes[10],((new c_R171)->m_new()));
	gc_assign(bb_reflection__classes[11],((new c_R174)->m_new()));
	gc_assign(bb_reflection__classes[12],((new c_R175)->m_new()));
	gc_assign(bb_reflection__classes[13],((new c_R209)->m_new()));
	gc_assign(bb_reflection__classes[14],((new c_R214)->m_new()));
	gc_assign(bb_reflection__classes[15],((new c_R225)->m_new()));
	gc_assign(bb_reflection__classes[16],((new c_R228)->m_new()));
	gc_assign(bb_reflection__classes[17],((new c_R262)->m_new()));
	gc_assign(bb_reflection__classes[18],((new c_R267)->m_new()));
	gc_assign(bb_reflection__classes[19],((new c_R278)->m_new()));
	gc_assign(bb_reflection__classes[20],((new c_R281)->m_new()));
	gc_assign(bb_reflection__classes[21],((new c_R315)->m_new()));
	gc_assign(bb_reflection__classes[22],((new c_R321)->m_new()));
	gc_assign(bb_reflection__classes[23],((new c_R332)->m_new()));
	gc_assign(bb_reflection__classes[24],((new c_R335)->m_new()));
	gc_assign(bb_reflection__classes[25],((new c_R342)->m_new()));
	gc_assign(bb_reflection__classes[26],((new c_R353)->m_new()));
	gc_assign(bb_reflection__classes[27],((new c_R355)->m_new()));
	gc_assign(bb_reflection__classes[28],((new c_R381)->m_new()));
	gc_assign(bb_reflection__classes[29],((new c_R384)->m_new()));
	gc_assign(bb_reflection__classes[30],((new c_R395)->m_new()));
	gc_assign(bb_reflection__classes[31],((new c_R397)->m_new()));
	gc_assign(bb_reflection__classes[32],((new c_R423)->m_new()));
	gc_assign(bb_reflection__classes[33],((new c_R426)->m_new()));
	gc_assign(bb_reflection__classes[34],((new c_R437)->m_new()));
	gc_assign(bb_reflection__classes[35],((new c_R439)->m_new()));
	gc_assign(bb_reflection__classes[36],((new c_R465)->m_new()));
	gc_assign(bb_reflection__classes[37],((new c_R468)->m_new()));
	gc_assign(bb_reflection__classes[38],((new c_R505)->m_new()));
	gc_assign(bb_reflection__classes[39],((new c_R510)->m_new()));
	gc_assign(bb_reflection__classes[40],((new c_R547)->m_new()));
	gc_assign(bb_reflection__classes[41],((new c_R552)->m_new()));
	gc_assign(bb_reflection__classes[42],((new c_R589)->m_new()));
	gc_assign(bb_reflection__classes[43],((new c_R595)->m_new()));
	gc_assign(bb_reflection__classes[44],((new c_R601)->m_new()));
	gc_assign(bb_reflection__classes[45],((new c_R651)->m_new()));
	gc_assign(bb_reflection__classes[46],((new c_R665)->m_new()));
	gc_assign(bb_reflection__classes[47],((new c_R705)->m_new()));
	gc_assign(bb_reflection__classes[48],((new c_R722)->m_new()));
	gc_assign(bb_reflection__classes[49],((new c_R748)->m_new()));
	gc_assign(bb_reflection__classes[50],((new c_R751)->m_new()));
	gc_assign(bb_reflection__classes[51],((new c_R766)->m_new()));
	gc_assign(bb_reflection__classes[52],((new c_R771)->m_new()));
	gc_assign(bb_reflection__classes[53],((new c_R777)->m_new()));
	gc_assign(bb_reflection__classes[54],((new c_R789)->m_new()));
	gc_assign(bb_reflection__classes[55],((new c_R809)->m_new()));
	gc_assign(bb_reflection__classes[56],((new c_R821)->m_new()));
	gc_assign(bb_reflection__classes[57],((new c_R843)->m_new()));
	gc_assign(bb_reflection__classes[58],((new c_R850)->m_new()));
	gc_assign(bb_reflection__classes[59],((new c_R857)->m_new()));
	gc_assign(bb_reflection__classes[60],((new c_R864)->m_new()));
	gc_assign(bb_reflection__classes[61],((new c_R871)->m_new()));
	gc_assign(bb_reflection__classes[62],((new c_R876)->m_new()));
	gc_assign(bb_reflection__classes[63],((new c_R883)->m_new()));
	gc_assign(bb_reflection__classes[64],((new c_R888)->m_new()));
	gc_assign(bb_reflection__classes[65],((new c_R893)->m_new()));
	gc_assign(bb_reflection__classes[66],((new c_R900)->m_new()));
	gc_assign(bb_reflection__classes[67],((new c_R907)->m_new()));
	gc_assign(bb_reflection__classes[68],((new c_R914)->m_new()));
	gc_assign(bb_reflection__classes[69],((new c_R929)->m_new()));
	gc_assign(bb_reflection__classes[70],((new c_R934)->m_new()));
	gc_assign(bb_reflection__classes[71],((new c_R939)->m_new()));
	gc_assign(bb_reflection__classes[72],((new c_R945)->m_new()));
	gc_assign(bb_reflection__classes[73],((new c_R960)->m_new()));
	gc_assign(bb_reflection__classes[74],((new c_R965)->m_new()));
	gc_assign(bb_reflection__classes[75],((new c_R970)->m_new()));
	gc_assign(bb_reflection__classes[76],((new c_R976)->m_new()));
	gc_assign(bb_reflection__classes[77],((new c_R991)->m_new()));
	gc_assign(bb_reflection__classes[78],((new c_R996)->m_new()));
	gc_assign(bb_reflection__classes[79],((new c_R1001)->m_new()));
	gc_assign(bb_reflection__classes[80],((new c_R1007)->m_new()));
	gc_assign(bb_reflection__classes[81],((new c_R1012)->m_new()));
	gc_assign(bb_reflection__classes[82],((new c_R1018)->m_new()));
	gc_assign(bb_reflection__classes[83],((new c_R1024)->m_new()));
	gc_assign(bb_reflection__classes[84],((new c_R1030)->m_new()));
	gc_assign(bb_reflection__classes[85],((new c_R1036)->m_new()));
	gc_assign(bb_reflection__classes[86],((new c_R1042)->m_new()));
	gc_assign(bb_reflection__classes[87],((new c_R1048)->m_new()));
	gc_assign(bb_reflection__classes[88],((new c_R1054)->m_new()));
	gc_assign(bb_reflection__classes[89],((new c_R1060)->m_new()));
	gc_assign(bb_reflection__classes[90],((new c_R1067)->m_new()));
	gc_assign(bb_reflection__classes[91],((new c_R1072)->m_new()));
	gc_assign(bb_reflection__classes[92],((new c_R1079)->m_new()));
	gc_assign(bb_reflection__classes[93],((new c_R1084)->m_new()));
	gc_assign(bb_reflection__classes[94],((new c_R1091)->m_new()));
	gc_assign(bb_reflection__classes[95],((new c_R1096)->m_new()));
	gc_assign(bb_reflection__classes[96],((new c_R1103)->m_new()));
	gc_assign(bb_reflection__classes[97],((new c_R1110)->m_new()));
	gc_assign(bb_reflection__classes[98],((new c_R1117)->m_new()));
	gc_assign(bb_reflection__classes[99],((new c_R1122)->m_new()));
	gc_assign(bb_reflection__classes[100],((new c_R1127)->m_new()));
	bb_reflection__classes[0]->p_Init3();
	bb_reflection__classes[1]->p_Init3();
	bb_reflection__classes[2]->p_Init3();
	bb_reflection__classes[3]->p_Init3();
	bb_reflection__classes[4]->p_Init3();
	bb_reflection__classes[5]->p_Init3();
	bb_reflection__classes[6]->p_Init3();
	bb_reflection__classes[7]->p_Init3();
	bb_reflection__classes[8]->p_Init3();
	bb_reflection__classes[9]->p_Init3();
	bb_reflection__classes[10]->p_Init3();
	bb_reflection__classes[11]->p_Init3();
	bb_reflection__classes[12]->p_Init3();
	bb_reflection__classes[13]->p_Init3();
	bb_reflection__classes[14]->p_Init3();
	bb_reflection__classes[15]->p_Init3();
	bb_reflection__classes[16]->p_Init3();
	bb_reflection__classes[17]->p_Init3();
	bb_reflection__classes[18]->p_Init3();
	bb_reflection__classes[19]->p_Init3();
	bb_reflection__classes[20]->p_Init3();
	bb_reflection__classes[21]->p_Init3();
	bb_reflection__classes[22]->p_Init3();
	bb_reflection__classes[23]->p_Init3();
	bb_reflection__classes[24]->p_Init3();
	bb_reflection__classes[25]->p_Init3();
	bb_reflection__classes[26]->p_Init3();
	bb_reflection__classes[27]->p_Init3();
	bb_reflection__classes[28]->p_Init3();
	bb_reflection__classes[29]->p_Init3();
	bb_reflection__classes[30]->p_Init3();
	bb_reflection__classes[31]->p_Init3();
	bb_reflection__classes[32]->p_Init3();
	bb_reflection__classes[33]->p_Init3();
	bb_reflection__classes[34]->p_Init3();
	bb_reflection__classes[35]->p_Init3();
	bb_reflection__classes[36]->p_Init3();
	bb_reflection__classes[37]->p_Init3();
	bb_reflection__classes[38]->p_Init3();
	bb_reflection__classes[39]->p_Init3();
	bb_reflection__classes[40]->p_Init3();
	bb_reflection__classes[41]->p_Init3();
	bb_reflection__classes[42]->p_Init3();
	bb_reflection__classes[43]->p_Init3();
	bb_reflection__classes[44]->p_Init3();
	bb_reflection__classes[45]->p_Init3();
	bb_reflection__classes[46]->p_Init3();
	bb_reflection__classes[47]->p_Init3();
	bb_reflection__classes[48]->p_Init3();
	bb_reflection__classes[49]->p_Init3();
	bb_reflection__classes[50]->p_Init3();
	bb_reflection__classes[51]->p_Init3();
	bb_reflection__classes[52]->p_Init3();
	bb_reflection__classes[53]->p_Init3();
	bb_reflection__classes[54]->p_Init3();
	bb_reflection__classes[55]->p_Init3();
	bb_reflection__classes[56]->p_Init3();
	bb_reflection__classes[57]->p_Init3();
	bb_reflection__classes[58]->p_Init3();
	bb_reflection__classes[59]->p_Init3();
	bb_reflection__classes[60]->p_Init3();
	bb_reflection__classes[61]->p_Init3();
	bb_reflection__classes[62]->p_Init3();
	bb_reflection__classes[63]->p_Init3();
	bb_reflection__classes[64]->p_Init3();
	bb_reflection__classes[65]->p_Init3();
	bb_reflection__classes[66]->p_Init3();
	bb_reflection__classes[67]->p_Init3();
	bb_reflection__classes[68]->p_Init3();
	bb_reflection__classes[69]->p_Init3();
	bb_reflection__classes[70]->p_Init3();
	bb_reflection__classes[71]->p_Init3();
	bb_reflection__classes[72]->p_Init3();
	bb_reflection__classes[73]->p_Init3();
	bb_reflection__classes[74]->p_Init3();
	bb_reflection__classes[75]->p_Init3();
	bb_reflection__classes[76]->p_Init3();
	bb_reflection__classes[77]->p_Init3();
	bb_reflection__classes[78]->p_Init3();
	bb_reflection__classes[79]->p_Init3();
	bb_reflection__classes[80]->p_Init3();
	bb_reflection__classes[81]->p_Init3();
	bb_reflection__classes[82]->p_Init3();
	bb_reflection__classes[83]->p_Init3();
	bb_reflection__classes[84]->p_Init3();
	bb_reflection__classes[85]->p_Init3();
	bb_reflection__classes[86]->p_Init3();
	bb_reflection__classes[87]->p_Init3();
	bb_reflection__classes[88]->p_Init3();
	bb_reflection__classes[89]->p_Init3();
	bb_reflection__classes[90]->p_Init3();
	bb_reflection__classes[91]->p_Init3();
	bb_reflection__classes[92]->p_Init3();
	bb_reflection__classes[93]->p_Init3();
	bb_reflection__classes[94]->p_Init3();
	bb_reflection__classes[95]->p_Init3();
	bb_reflection__classes[96]->p_Init3();
	bb_reflection__classes[97]->p_Init3();
	bb_reflection__classes[98]->p_Init3();
	bb_reflection__classes[99]->p_Init3();
	bb_reflection__classes[100]->p_Init3();
	gc_assign(bb_reflection__consts,Array<c_ConstInfo* >(5));
	gc_assign(bb_reflection__consts[0],(new c_ConstInfo)->m_new(String(L"monkey.math.PI",14),0,bb_reflection__floatClass,((new c_FloatObject)->m_new2(FLOAT(3.14159265)))));
	gc_assign(bb_reflection__consts[1],(new c_ConstInfo)->m_new(String(L"monkey.math.TWOPI",17),0,bb_reflection__floatClass,((new c_FloatObject)->m_new2(FLOAT(6.28318531)))));
	gc_assign(bb_reflection__consts[2],(new c_ConstInfo)->m_new(String(L"monkey.math.HALFPI",18),0,bb_reflection__floatClass,((new c_FloatObject)->m_new2(FLOAT(1.57079633)))));
	gc_assign(bb_reflection__consts[3],(new c_ConstInfo)->m_new(String(L"monkey.random.A",15),2,bb_reflection__intClass,((new c_IntObject)->m_new(1664525))));
	gc_assign(bb_reflection__consts[4],(new c_ConstInfo)->m_new(String(L"monkey.random.C",15),2,bb_reflection__intClass,((new c_IntObject)->m_new(1013904223))));
	gc_assign(bb_reflection__globals,Array<c_GlobalInfo* >(1));
	gc_assign(bb_reflection__globals[0],((new c_R59)->m_new()));
	gc_assign(bb_reflection__functions,Array<c_FunctionInfo* >(45));
	gc_assign(bb_reflection__functions[0],((new c_R17)->m_new()));
	gc_assign(bb_reflection__functions[1],((new c_R18)->m_new()));
	gc_assign(bb_reflection__functions[2],((new c_R19)->m_new()));
	gc_assign(bb_reflection__functions[3],((new c_R20)->m_new()));
	gc_assign(bb_reflection__functions[4],((new c_R21)->m_new()));
	gc_assign(bb_reflection__functions[5],((new c_R22)->m_new()));
	gc_assign(bb_reflection__functions[6],((new c_R23)->m_new()));
	gc_assign(bb_reflection__functions[7],((new c_R24)->m_new()));
	gc_assign(bb_reflection__functions[8],((new c_R25)->m_new()));
	gc_assign(bb_reflection__functions[9],((new c_R26)->m_new()));
	gc_assign(bb_reflection__functions[10],((new c_R27)->m_new()));
	gc_assign(bb_reflection__functions[11],((new c_R28)->m_new()));
	gc_assign(bb_reflection__functions[12],((new c_R29)->m_new()));
	gc_assign(bb_reflection__functions[13],((new c_R30)->m_new()));
	gc_assign(bb_reflection__functions[14],((new c_R31)->m_new()));
	gc_assign(bb_reflection__functions[15],((new c_R32)->m_new()));
	gc_assign(bb_reflection__functions[16],((new c_R33)->m_new()));
	gc_assign(bb_reflection__functions[17],((new c_R34)->m_new()));
	gc_assign(bb_reflection__functions[18],((new c_R35)->m_new()));
	gc_assign(bb_reflection__functions[19],((new c_R36)->m_new()));
	gc_assign(bb_reflection__functions[20],((new c_R37)->m_new()));
	gc_assign(bb_reflection__functions[21],((new c_R38)->m_new()));
	gc_assign(bb_reflection__functions[22],((new c_R39)->m_new()));
	gc_assign(bb_reflection__functions[23],((new c_R40)->m_new()));
	gc_assign(bb_reflection__functions[24],((new c_R41)->m_new()));
	gc_assign(bb_reflection__functions[25],((new c_R42)->m_new()));
	gc_assign(bb_reflection__functions[26],((new c_R43)->m_new()));
	gc_assign(bb_reflection__functions[27],((new c_R44)->m_new()));
	gc_assign(bb_reflection__functions[28],((new c_R45)->m_new()));
	gc_assign(bb_reflection__functions[29],((new c_R46)->m_new()));
	gc_assign(bb_reflection__functions[30],((new c_R47)->m_new()));
	gc_assign(bb_reflection__functions[31],((new c_R48)->m_new()));
	gc_assign(bb_reflection__functions[32],((new c_R49)->m_new()));
	gc_assign(bb_reflection__functions[33],((new c_R50)->m_new()));
	gc_assign(bb_reflection__functions[34],((new c_R51)->m_new()));
	gc_assign(bb_reflection__functions[35],((new c_R52)->m_new()));
	gc_assign(bb_reflection__functions[36],((new c_R53)->m_new()));
	gc_assign(bb_reflection__functions[37],((new c_R54)->m_new()));
	gc_assign(bb_reflection__functions[38],((new c_R55)->m_new()));
	gc_assign(bb_reflection__functions[39],((new c_R56)->m_new()));
	gc_assign(bb_reflection__functions[40],((new c_R57)->m_new()));
	gc_assign(bb_reflection__functions[41],((new c_R58)->m_new()));
	gc_assign(bb_reflection__functions[42],((new c_R60)->m_new()));
	gc_assign(bb_reflection__functions[43],((new c_R61)->m_new()));
	gc_assign(bb_reflection__functions[44],((new c_R62)->m_new()));
	gc_assign(bb_reflection__getClass,((new c___GetClass)->m_new()));
	return 0;
}
int bb_reflection__init;
c_App::c_App(){
}
c_App* c_App::m_new(){
	if((bb_app__app)!=0){
		bbError(String(L"App has already been created",28));
	}
	gc_assign(bb_app__app,this);
	gc_assign(bb_app__delegate,(new c_GameDelegate)->m_new());
	bb_app__game->SetDelegate(bb_app__delegate);
	return this;
}
int c_App::p_OnResize(){
	return 0;
}
int c_App::p_OnCreate(){
	return 0;
}
int c_App::p_OnSuspend(){
	return 0;
}
int c_App::p_OnResume(){
	return 0;
}
int c_App::p_OnUpdate(){
	return 0;
}
int c_App::p_OnLoading(){
	return 0;
}
int c_App::p_OnRender(){
	return 0;
}
int c_App::p_OnClose(){
	bb_app_EndApp();
	return 0;
}
int c_App::p_OnBack(){
	p_OnClose();
	return 0;
}
void c_App::mark(){
	Object::mark();
}
c_VsatApp::c_VsatApp(){
	m_displayFps=false;
	m_activeScene=0;
	m_transition=0;
	m_screenWidth=0;
	m_screenHeight=0;
	m_screenWidth2=0;
	m_screenHeight2=0;
	m_systemFont=0;
	m_paused=false;
	m_lastUpdate=FLOAT(.0);
	m_deltaTime=FLOAT(.0);
	m_seconds=FLOAT(.0);
	m_nextScene=0;
}
c_VsatApp* c_VsatApp::m_new(){
	c_App::m_new();
	return this;
}
void c_VsatApp::p_ChangeScene(c_VScene* t_scene){
	bb_functions2_Assert(t_scene);
	try{
		if((m_activeScene)!=0){
			m_activeScene->p_OnExit();
			t_scene->p_OnInit();
			gc_assign(m_activeScene,t_scene);
		}else{
			c_DummyScene* t_dummy=(new c_DummyScene)->m_new();
			t_dummy->p_InitWithScene(t_scene);
			gc_assign(m_activeScene,(t_dummy));
		}
	}catch(c_Exception* t_e){
		bbError(t_e->p_ToString());
	}
}
void c_VsatApp::p_StartFadeIn(c_VTransition* t_transition){
	bb_functions2_Assert2(((t_transition)!=0) && t_transition->p_Duration2()>FLOAT(0.0));
	gc_assign(this->m_transition,t_transition);
}
int c_VsatApp::p_OnLoading(){
	try{
		if((m_activeScene)!=0){
			m_activeScene->p_OnLoading();
		}
	}catch(c_Exception* t_e){
		bbError(t_e->p_ToString());
	}
	return 0;
}
void c_VsatApp::p_UpdateScreenSize(){
	m_screenWidth=bb_app_DeviceWidth();
	m_screenHeight=bb_app_DeviceHeight();
	m_screenWidth2=int(Float(m_screenWidth)/FLOAT(2.0));
	m_screenHeight2=int(Float(m_screenHeight)/FLOAT(2.0));
}
int c_VsatApp::p_OnCreate(){
	try{
		p_UpdateScreenSize();
		bb_app_SetUpdateRate(0);
		gc_assign(m_systemFont,c_FontCache::m_GetFont(String(L"sans",4)));
	}catch(c_Exception* t_e){
		bbError(t_e->p_ToString());
	}
	return 0;
}
void c_VsatApp::p_UpdateGameTime(){
	Float t_now=Float(bb_app_Millisecs());
	m_deltaTime=(t_now-m_lastUpdate)/FLOAT(1000.0);
	m_seconds+=m_deltaTime;
	m_lastUpdate=t_now;
}
int c_VsatApp::p_OnUpdate(){
	if(m_paused){
		if((m_activeScene)!=0){
			try{
				m_activeScene->p_OnUpdateWhilePaused();
			}catch(c_Exception* t_e){
				bbError(t_e->p_ToString());
			}
		}
		return 0;
	}
	try{
		p_UpdateGameTime();
		if((m_transition)!=0){
			m_transition->p_Update4(m_deltaTime);
			if(!m_transition->p_IsActive()){
				m_transition=0;
				if((m_nextScene)!=0){
					p_ChangeScene(m_nextScene);
					m_nextScene=0;
				}
			}
		}
		if((m_activeScene)!=0){
			m_activeScene->p_OnUpdate2(m_deltaTime);
		}
	}catch(c_Exception* t_e2){
		bbError(t_e2->p_ToString());
	}
	return 0;
}
void c_VsatApp::p_RenderFps(){
	if((m_systemFont)!=0){
		bb_graphics_PushMatrix();
		bb_functions_ResetMatrix();
		c_Color::m_White->p_Use();
		m_systemFont->p_DrawText2(String(L"Fps: ",5)+String(bb_fps_GetFps()),m_screenWidth-4,2,2,3);
		bb_graphics_PopMatrix();
	}
}
int c_VsatApp::p_OnRender(){
	try{
		if((m_activeScene)!=0){
			m_activeScene->p_OnRender();
		}
		if((m_transition)!=0){
			m_transition->p_Render();
		}
	}catch(c_Exception* t_e){
		bbError(t_e->p_ToString());
	}
	bb_fps_UpdateFps();
	if(m_displayFps){
		p_RenderFps();
	}
	return 0;
}
int c_VsatApp::p_OnSuspend(){
	try{
		if((m_activeScene)!=0){
			m_activeScene->p_OnSuspend();
		}
	}catch(c_Exception* t_e){
		bbError(t_e->p_ToString());
	}
	return 0;
}
int c_VsatApp::p_OnResume(){
	try{
		if((m_activeScene)!=0){
			m_activeScene->p_OnResume();
		}
	}catch(c_Exception* t_e){
		bbError(t_e->p_ToString());
	}
	return 0;
}
int c_VsatApp::p_OnResize(){
	try{
		p_UpdateScreenSize();
		if((m_activeScene)!=0){
			m_activeScene->p_OnResize();
		}
	}catch(c_Exception* t_e){
		bbError(t_e->p_ToString());
	}
	return 0;
}
int c_VsatApp::p_ScreenWidth(){
	return m_screenWidth;
}
int c_VsatApp::p_ScreenHeight(){
	return m_screenHeight;
}
void c_VsatApp::mark(){
	c_App::mark();
	gc_mark_q(m_activeScene);
	gc_mark_q(m_transition);
	gc_mark_q(m_systemFont);
	gc_mark_q(m_nextScene);
}
c_App* bb_app__app;
c_GameDelegate::c_GameDelegate(){
	m__graphics=0;
	m__audio=0;
	m__input=0;
}
c_GameDelegate* c_GameDelegate::m_new(){
	return this;
}
void c_GameDelegate::StartGame(){
	gc_assign(m__graphics,(new gxtkGraphics));
	bb_graphics_SetGraphicsDevice(m__graphics);
	bb_graphics_SetFont(0,32);
	gc_assign(m__audio,(new gxtkAudio));
	bb_audio_SetAudioDevice(m__audio);
	gc_assign(m__input,(new c_InputDevice)->m_new());
	bb_input_SetInputDevice(m__input);
	bb_app_ValidateDeviceWindow(false);
	bb_app_EnumDisplayModes();
	bb_app__app->p_OnCreate();
}
void c_GameDelegate::SuspendGame(){
	bb_app__app->p_OnSuspend();
	m__audio->Suspend();
}
void c_GameDelegate::ResumeGame(){
	m__audio->Resume();
	bb_app__app->p_OnResume();
}
void c_GameDelegate::UpdateGame(){
	bb_app_ValidateDeviceWindow(true);
	m__input->p_BeginUpdate();
	bb_app__app->p_OnUpdate();
	m__input->p_EndUpdate();
}
void c_GameDelegate::RenderGame(){
	bb_app_ValidateDeviceWindow(true);
	int t_mode=m__graphics->BeginRender();
	if((t_mode)!=0){
		bb_graphics_BeginRender();
	}
	if(t_mode==2){
		bb_app__app->p_OnLoading();
	}else{
		bb_app__app->p_OnRender();
	}
	if((t_mode)!=0){
		bb_graphics_EndRender();
	}
	m__graphics->EndRender();
}
void c_GameDelegate::KeyEvent(int t_event,int t_data){
	m__input->p_KeyEvent(t_event,t_data);
	if(t_event!=1){
		return;
	}
	int t_1=t_data;
	if(t_1==432){
		bb_app__app->p_OnClose();
	}else{
		if(t_1==416){
			bb_app__app->p_OnBack();
		}
	}
}
void c_GameDelegate::MouseEvent(int t_event,int t_data,Float t_x,Float t_y){
	m__input->p_MouseEvent(t_event,t_data,t_x,t_y);
}
void c_GameDelegate::TouchEvent(int t_event,int t_data,Float t_x,Float t_y){
	m__input->p_TouchEvent(t_event,t_data,t_x,t_y);
}
void c_GameDelegate::MotionEvent(int t_event,int t_data,Float t_x,Float t_y,Float t_z){
	m__input->p_MotionEvent(t_event,t_data,t_x,t_y,t_z);
}
void c_GameDelegate::DiscardGraphics(){
	m__graphics->DiscardGraphics();
}
void c_GameDelegate::mark(){
	BBGameDelegate::mark();
	gc_mark_q(m__graphics);
	gc_mark_q(m__audio);
	gc_mark_q(m__input);
}
c_GameDelegate* bb_app__delegate;
BBGame* bb_app__game;
c_VsatApp* bb_app2_Vsat;
c_VScene::c_VScene(){
}
c_VScene* c_VScene::m_new(){
	return this;
}
void c_VScene::p_OnExit(){
}
void c_VScene::p_OnInit(){
}
void c_VScene::p_OnLoading(){
}
void c_VScene::p_OnUpdateWhilePaused(){
}
void c_VScene::p_OnUpdate2(Float t_delta){
}
void c_VScene::p_OnRender(){
}
void c_VScene::p_OnSuspend(){
}
void c_VScene::p_OnResume(){
}
void c_VScene::p_OnResize(){
}
void c_VScene::mark(){
	Object::mark();
}
c_vvv::c_vvv(){
	m_shapes=(new c_List4)->m_new();
	m_x=0;
	m_actions=(new c_List6)->m_new();
}
c_vvv* c_vvv::m_new(){
	c_VScene::m_new();
	return this;
}
void c_vvv::p_OnInit(){
	c_VFadeInLinear* t_fade=(new c_VFadeInLinear)->m_new2(FLOAT(0.5));
	t_fade->p_SetColor2(c_Color::m_Teal);
	bb_app2_Vsat->p_StartFadeIn(t_fade);
	c_VCircle* t_a=(new c_VCircle)->m_new(FLOAT(100.0),FLOAT(100.0),FLOAT(30.0));
	t_a->m_renderOutline=true;
	c_VRect* t_b=(new c_VRect)->m_new(FLOAT(300.0),FLOAT(200.0),FLOAT(20.0),FLOAT(100.0));
	m_shapes->p_AddLast4(t_a);
	m_shapes->p_AddLast4(t_b);
}
void c_vvv::p_OnUpdate2(Float t_dt){
	m_x+=1;
	if((bb_input_KeyHit(39))!=0){
		c_VVec2Action* t_move=(new c_VVec2Action)->m_new(m_shapes->p_First()->m_scale,FLOAT(4.0),FLOAT(4.0),FLOAT(1.0),26,true);
		t_move->p_AddToList(m_actions);
		t_move->p_SetListener(this);
	}
	c_Enumerator11* t_=m_actions->p_ObjectEnumerator();
	while(t_->p_HasNext()){
		c_VAction* t_a=t_->p_NextObject();
		t_a->p_Update4(t_dt);
	}
}
void c_vvv::p_OnRender(){
	bb_functions_ClearScreenWithColor(c_Color::m_Navy);
	c_Enumerator12* t_=m_shapes->p_ObjectEnumerator();
	while(t_->p_HasNext()){
		c_VShape* t_a=t_->p_NextObject();
		t_a->p_Render();
	}
}
void c_vvv::p_OnActionEvent(int t_id,c_VAction* t_action){
	int t_1=t_id;
	if(t_1==1){
		bbPrint(String(L"Start",5));
	}else{
		if(t_1==2){
			bbPrint(String(L"Done",4));
		}
	}
}
void c_vvv::mark(){
	c_VScene::mark();
	gc_mark_q(m_shapes);
	gc_mark_q(m_actions);
}
void bb_functions2_Assert(Object* t_obj){
}
void bb_functions2_Assert2(bool t_assumption){
}
c_DummyScene::c_DummyScene(){
	m_initScene=0;
}
c_DummyScene* c_DummyScene::m_new(){
	c_VScene::m_new();
	return this;
}
void c_DummyScene::p_InitWithScene(c_VScene* t_scene){
	bb_functions2_Assert(t_scene);
	gc_assign(m_initScene,t_scene);
}
void c_DummyScene::p_OnUpdate2(Float t_dt){
	if(!((bb_app2_Vsat)!=0)){
		throw (new c_Exception)->m_new2(String(L"Call 'Vsat = New VsatApp' before changing scenes.",49));
	}
	bb_app2_Vsat->p_ChangeScene(m_initScene);
}
void c_DummyScene::mark(){
	c_VScene::mark();
	gc_mark_q(m_initScene);
}
int bbMain(){
	gc_assign(bb_app2_Vsat,(new c_VsatApp)->m_new());
	bb_app2_Vsat->m_displayFps=true;
	bb_app2_Vsat->p_ChangeScene((new c_vvv)->m_new());
	return 0;
}
c_Stack4::c_Stack4(){
	m_data=Array<c_ConstInfo* >();
	m_length=0;
}
c_Stack4* c_Stack4::m_new(){
	return this;
}
c_Stack4* c_Stack4::m_new2(Array<c_ConstInfo* > t_data){
	gc_assign(this->m_data,t_data.Slice(0));
	this->m_length=t_data.Length();
	return this;
}
void c_Stack4::p_Push10(c_ConstInfo* t_value){
	if(m_length==m_data.Length()){
		gc_assign(m_data,m_data.Resize(m_length*2+10));
	}
	gc_assign(m_data[m_length],t_value);
	m_length+=1;
}
void c_Stack4::p_Push11(Array<c_ConstInfo* > t_values,int t_offset,int t_count){
	for(int t_i=0;t_i<t_count;t_i=t_i+1){
		p_Push10(t_values[t_offset+t_i]);
	}
}
void c_Stack4::p_Push12(Array<c_ConstInfo* > t_values,int t_offset){
	p_Push11(t_values,t_offset,t_values.Length()-t_offset);
}
Array<c_ConstInfo* > c_Stack4::p_ToArray(){
	Array<c_ConstInfo* > t_t=Array<c_ConstInfo* >(m_length);
	for(int t_i=0;t_i<m_length;t_i=t_i+1){
		gc_assign(t_t[t_i],m_data[t_i]);
	}
	return t_t;
}
void c_Stack4::mark(){
	Object::mark();
	gc_mark_q(m_data);
}
c_FieldInfo::c_FieldInfo(){
	m__name=String();
	m__attrs=0;
	m__type=0;
}
c_FieldInfo* c_FieldInfo::m_new(String t_name,int t_attrs,c_ClassInfo* t_type){
	m__name=t_name;
	m__attrs=t_attrs;
	gc_assign(m__type,t_type);
	return this;
}
c_FieldInfo* c_FieldInfo::m_new2(){
	return this;
}
void c_FieldInfo::mark(){
	Object::mark();
	gc_mark_q(m__type);
}
c_Stack5::c_Stack5(){
	m_data=Array<c_FieldInfo* >();
	m_length=0;
}
c_Stack5* c_Stack5::m_new(){
	return this;
}
c_Stack5* c_Stack5::m_new2(Array<c_FieldInfo* > t_data){
	gc_assign(this->m_data,t_data.Slice(0));
	this->m_length=t_data.Length();
	return this;
}
void c_Stack5::p_Push13(c_FieldInfo* t_value){
	if(m_length==m_data.Length()){
		gc_assign(m_data,m_data.Resize(m_length*2+10));
	}
	gc_assign(m_data[m_length],t_value);
	m_length+=1;
}
void c_Stack5::p_Push14(Array<c_FieldInfo* > t_values,int t_offset,int t_count){
	for(int t_i=0;t_i<t_count;t_i=t_i+1){
		p_Push13(t_values[t_offset+t_i]);
	}
}
void c_Stack5::p_Push15(Array<c_FieldInfo* > t_values,int t_offset){
	p_Push14(t_values,t_offset,t_values.Length()-t_offset);
}
Array<c_FieldInfo* > c_Stack5::p_ToArray(){
	Array<c_FieldInfo* > t_t=Array<c_FieldInfo* >(m_length);
	for(int t_i=0;t_i<m_length;t_i=t_i+1){
		gc_assign(t_t[t_i],m_data[t_i]);
	}
	return t_t;
}
void c_Stack5::mark(){
	Object::mark();
	gc_mark_q(m_data);
}
c_Stack6::c_Stack6(){
	m_data=Array<c_GlobalInfo* >();
	m_length=0;
}
c_Stack6* c_Stack6::m_new(){
	return this;
}
c_Stack6* c_Stack6::m_new2(Array<c_GlobalInfo* > t_data){
	gc_assign(this->m_data,t_data.Slice(0));
	this->m_length=t_data.Length();
	return this;
}
void c_Stack6::p_Push16(c_GlobalInfo* t_value){
	if(m_length==m_data.Length()){
		gc_assign(m_data,m_data.Resize(m_length*2+10));
	}
	gc_assign(m_data[m_length],t_value);
	m_length+=1;
}
void c_Stack6::p_Push17(Array<c_GlobalInfo* > t_values,int t_offset,int t_count){
	for(int t_i=0;t_i<t_count;t_i=t_i+1){
		p_Push16(t_values[t_offset+t_i]);
	}
}
void c_Stack6::p_Push18(Array<c_GlobalInfo* > t_values,int t_offset){
	p_Push17(t_values,t_offset,t_values.Length()-t_offset);
}
Array<c_GlobalInfo* > c_Stack6::p_ToArray(){
	Array<c_GlobalInfo* > t_t=Array<c_GlobalInfo* >(m_length);
	for(int t_i=0;t_i<m_length;t_i=t_i+1){
		gc_assign(t_t[t_i],m_data[t_i]);
	}
	return t_t;
}
void c_Stack6::mark(){
	Object::mark();
	gc_mark_q(m_data);
}
c_MethodInfo::c_MethodInfo(){
	m__name=String();
	m__attrs=0;
	m__retType=0;
	m__argTypes=Array<c_ClassInfo* >();
}
c_MethodInfo* c_MethodInfo::m_new(String t_name,int t_attrs,c_ClassInfo* t_retType,Array<c_ClassInfo* > t_argTypes){
	m__name=t_name;
	m__attrs=t_attrs;
	gc_assign(m__retType,t_retType);
	gc_assign(m__argTypes,t_argTypes);
	return this;
}
c_MethodInfo* c_MethodInfo::m_new2(){
	return this;
}
void c_MethodInfo::mark(){
	Object::mark();
	gc_mark_q(m__retType);
	gc_mark_q(m__argTypes);
}
c_Stack7::c_Stack7(){
	m_data=Array<c_MethodInfo* >();
	m_length=0;
}
c_Stack7* c_Stack7::m_new(){
	return this;
}
c_Stack7* c_Stack7::m_new2(Array<c_MethodInfo* > t_data){
	gc_assign(this->m_data,t_data.Slice(0));
	this->m_length=t_data.Length();
	return this;
}
void c_Stack7::p_Push19(c_MethodInfo* t_value){
	if(m_length==m_data.Length()){
		gc_assign(m_data,m_data.Resize(m_length*2+10));
	}
	gc_assign(m_data[m_length],t_value);
	m_length+=1;
}
void c_Stack7::p_Push20(Array<c_MethodInfo* > t_values,int t_offset,int t_count){
	for(int t_i=0;t_i<t_count;t_i=t_i+1){
		p_Push19(t_values[t_offset+t_i]);
	}
}
void c_Stack7::p_Push21(Array<c_MethodInfo* > t_values,int t_offset){
	p_Push20(t_values,t_offset,t_values.Length()-t_offset);
}
Array<c_MethodInfo* > c_Stack7::p_ToArray(){
	Array<c_MethodInfo* > t_t=Array<c_MethodInfo* >(m_length);
	for(int t_i=0;t_i<m_length;t_i=t_i+1){
		gc_assign(t_t[t_i],m_data[t_i]);
	}
	return t_t;
}
void c_Stack7::mark(){
	Object::mark();
	gc_mark_q(m_data);
}
c_Stack8::c_Stack8(){
	m_data=Array<c_FunctionInfo* >();
	m_length=0;
}
c_Stack8* c_Stack8::m_new(){
	return this;
}
c_Stack8* c_Stack8::m_new2(Array<c_FunctionInfo* > t_data){
	gc_assign(this->m_data,t_data.Slice(0));
	this->m_length=t_data.Length();
	return this;
}
void c_Stack8::p_Push22(c_FunctionInfo* t_value){
	if(m_length==m_data.Length()){
		gc_assign(m_data,m_data.Resize(m_length*2+10));
	}
	gc_assign(m_data[m_length],t_value);
	m_length+=1;
}
void c_Stack8::p_Push23(Array<c_FunctionInfo* > t_values,int t_offset,int t_count){
	for(int t_i=0;t_i<t_count;t_i=t_i+1){
		p_Push22(t_values[t_offset+t_i]);
	}
}
void c_Stack8::p_Push24(Array<c_FunctionInfo* > t_values,int t_offset){
	p_Push23(t_values,t_offset,t_values.Length()-t_offset);
}
Array<c_FunctionInfo* > c_Stack8::p_ToArray(){
	Array<c_FunctionInfo* > t_t=Array<c_FunctionInfo* >(m_length);
	for(int t_i=0;t_i<m_length;t_i=t_i+1){
		gc_assign(t_t[t_i],m_data[t_i]);
	}
	return t_t;
}
void c_Stack8::mark(){
	Object::mark();
	gc_mark_q(m_data);
}
c_R65::c_R65(){
}
c_R65* c_R65::m_new(){
	c_FieldInfo::m_new(String(L"value",5),0,bb_reflection__boolClass);
	return this;
}
void c_R65::mark(){
	c_FieldInfo::mark();
}
c_R67::c_R67(){
}
c_R67* c_R67::m_new(){
	c_MethodInfo::m_new(String(L"ToBool",6),0,bb_reflection__boolClass,Array<c_ClassInfo* >());
	return this;
}
void c_R67::mark(){
	c_MethodInfo::mark();
}
c_R68::c_R68(){
}
c_R68* c_R68::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[1]};
	c_MethodInfo::m_new(String(L"Equals",6),0,bb_reflection__boolClass,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R68::mark(){
	c_MethodInfo::mark();
}
c_R66::c_R66(){
}
c_R66* c_R66::m_new(){
	c_ClassInfo* t_[]={bb_reflection__boolClass};
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[1],Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R66::mark(){
	c_FunctionInfo::mark();
}
c_R69::c_R69(){
}
c_R69* c_R69::m_new(){
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[1],Array<c_ClassInfo* >());
	return this;
}
void c_R69::mark(){
	c_FunctionInfo::mark();
}
c_R71::c_R71(){
}
c_R71* c_R71::m_new(){
	c_FieldInfo::m_new(String(L"value",5),0,bb_reflection__intClass);
	return this;
}
void c_R71::mark(){
	c_FieldInfo::mark();
}
c_R74::c_R74(){
}
c_R74* c_R74::m_new(){
	c_MethodInfo::m_new(String(L"ToInt",5),0,bb_reflection__intClass,Array<c_ClassInfo* >());
	return this;
}
void c_R74::mark(){
	c_MethodInfo::mark();
}
c_R75::c_R75(){
}
c_R75* c_R75::m_new(){
	c_MethodInfo::m_new(String(L"ToFloat",7),0,bb_reflection__floatClass,Array<c_ClassInfo* >());
	return this;
}
void c_R75::mark(){
	c_MethodInfo::mark();
}
c_R76::c_R76(){
}
c_R76* c_R76::m_new(){
	c_MethodInfo::m_new(String(L"ToString",8),0,bb_reflection__stringClass,Array<c_ClassInfo* >());
	return this;
}
void c_R76::mark(){
	c_MethodInfo::mark();
}
c_R77::c_R77(){
}
c_R77* c_R77::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[2]};
	c_MethodInfo::m_new(String(L"Equals",6),0,bb_reflection__boolClass,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R77::mark(){
	c_MethodInfo::mark();
}
c_R78::c_R78(){
}
c_R78* c_R78::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[2]};
	c_MethodInfo::m_new(String(L"Compare",7),0,bb_reflection__intClass,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R78::mark(){
	c_MethodInfo::mark();
}
c_R72::c_R72(){
}
c_R72* c_R72::m_new(){
	c_ClassInfo* t_[]={bb_reflection__intClass};
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[2],Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R72::mark(){
	c_FunctionInfo::mark();
}
c_R73::c_R73(){
}
c_R73* c_R73::m_new(){
	c_ClassInfo* t_[]={bb_reflection__floatClass};
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[2],Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R73::mark(){
	c_FunctionInfo::mark();
}
c_R79::c_R79(){
}
c_R79* c_R79::m_new(){
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[2],Array<c_ClassInfo* >());
	return this;
}
void c_R79::mark(){
	c_FunctionInfo::mark();
}
c_R81::c_R81(){
}
c_R81* c_R81::m_new(){
	c_FieldInfo::m_new(String(L"value",5),0,bb_reflection__floatClass);
	return this;
}
void c_R81::mark(){
	c_FieldInfo::mark();
}
c_R84::c_R84(){
}
c_R84* c_R84::m_new(){
	c_MethodInfo::m_new(String(L"ToInt",5),0,bb_reflection__intClass,Array<c_ClassInfo* >());
	return this;
}
void c_R84::mark(){
	c_MethodInfo::mark();
}
c_R85::c_R85(){
}
c_R85* c_R85::m_new(){
	c_MethodInfo::m_new(String(L"ToFloat",7),0,bb_reflection__floatClass,Array<c_ClassInfo* >());
	return this;
}
void c_R85::mark(){
	c_MethodInfo::mark();
}
c_R86::c_R86(){
}
c_R86* c_R86::m_new(){
	c_MethodInfo::m_new(String(L"ToString",8),0,bb_reflection__stringClass,Array<c_ClassInfo* >());
	return this;
}
void c_R86::mark(){
	c_MethodInfo::mark();
}
c_R87::c_R87(){
}
c_R87* c_R87::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[3]};
	c_MethodInfo::m_new(String(L"Equals",6),0,bb_reflection__boolClass,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R87::mark(){
	c_MethodInfo::mark();
}
c_R88::c_R88(){
}
c_R88* c_R88::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[3]};
	c_MethodInfo::m_new(String(L"Compare",7),0,bb_reflection__intClass,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R88::mark(){
	c_MethodInfo::mark();
}
c_R82::c_R82(){
}
c_R82* c_R82::m_new(){
	c_ClassInfo* t_[]={bb_reflection__intClass};
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[3],Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R82::mark(){
	c_FunctionInfo::mark();
}
c_R83::c_R83(){
}
c_R83* c_R83::m_new(){
	c_ClassInfo* t_[]={bb_reflection__floatClass};
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[3],Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R83::mark(){
	c_FunctionInfo::mark();
}
c_R89::c_R89(){
}
c_R89* c_R89::m_new(){
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[3],Array<c_ClassInfo* >());
	return this;
}
void c_R89::mark(){
	c_FunctionInfo::mark();
}
c_R91::c_R91(){
}
c_R91* c_R91::m_new(){
	c_FieldInfo::m_new(String(L"value",5),0,bb_reflection__stringClass);
	return this;
}
void c_R91::mark(){
	c_FieldInfo::mark();
}
c_R95::c_R95(){
}
c_R95* c_R95::m_new(){
	c_MethodInfo::m_new(String(L"ToString",8),0,bb_reflection__stringClass,Array<c_ClassInfo* >());
	return this;
}
void c_R95::mark(){
	c_MethodInfo::mark();
}
c_R96::c_R96(){
}
c_R96* c_R96::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[4]};
	c_MethodInfo::m_new(String(L"Equals",6),0,bb_reflection__boolClass,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R96::mark(){
	c_MethodInfo::mark();
}
c_R97::c_R97(){
}
c_R97* c_R97::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[4]};
	c_MethodInfo::m_new(String(L"Compare",7),0,bb_reflection__intClass,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R97::mark(){
	c_MethodInfo::mark();
}
c_R92::c_R92(){
}
c_R92* c_R92::m_new(){
	c_ClassInfo* t_[]={bb_reflection__intClass};
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[4],Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R92::mark(){
	c_FunctionInfo::mark();
}
c_R93::c_R93(){
}
c_R93* c_R93::m_new(){
	c_ClassInfo* t_[]={bb_reflection__floatClass};
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[4],Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R93::mark(){
	c_FunctionInfo::mark();
}
c_R94::c_R94(){
}
c_R94* c_R94::m_new(){
	c_ClassInfo* t_[]={bb_reflection__stringClass};
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[4],Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R94::mark(){
	c_FunctionInfo::mark();
}
c_R98::c_R98(){
}
c_R98* c_R98::m_new(){
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[4],Array<c_ClassInfo* >());
	return this;
}
void c_R98::mark(){
	c_FunctionInfo::mark();
}
c_R115::c_R115(){
}
c_R115* c_R115::m_new(){
	c_GlobalInfo::m_new(String(L"NIL",3),2,bb_reflection__intClass);
	return this;
}
void c_R115::mark(){
	c_GlobalInfo::mark();
}
c_R116::c_R116(){
}
c_R116* c_R116::m_new(){
	c_FieldInfo::m_new(String(L"_data",5),2,bb_reflection__classes[98]);
	return this;
}
void c_R116::mark(){
	c_FieldInfo::mark();
}
c_R117::c_R117(){
}
c_R117* c_R117::m_new(){
	c_FieldInfo::m_new(String(L"_capacity",9),2,bb_reflection__intClass);
	return this;
}
void c_R117::mark(){
	c_FieldInfo::mark();
}
c_R118::c_R118(){
}
c_R118* c_R118::m_new(){
	c_FieldInfo::m_new(String(L"_first",6),2,bb_reflection__intClass);
	return this;
}
void c_R118::mark(){
	c_FieldInfo::mark();
}
c_R119::c_R119(){
}
c_R119* c_R119::m_new(){
	c_FieldInfo::m_new(String(L"_last",5),2,bb_reflection__intClass);
	return this;
}
void c_R119::mark(){
	c_FieldInfo::mark();
}
c_R102::c_R102(){
}
c_R102* c_R102::m_new(){
	c_MethodInfo::m_new(String(L"Clear",5),0,0,Array<c_ClassInfo* >());
	return this;
}
void c_R102::mark(){
	c_MethodInfo::mark();
}
c_R103::c_R103(){
}
c_R103* c_R103::m_new(){
	c_MethodInfo::m_new(String(L"Length",6),0,bb_reflection__intClass,Array<c_ClassInfo* >());
	return this;
}
void c_R103::mark(){
	c_MethodInfo::mark();
}
c_R104::c_R104(){
}
c_R104* c_R104::m_new(){
	c_MethodInfo::m_new(String(L"IsEmpty",7),0,bb_reflection__boolClass,Array<c_ClassInfo* >());
	return this;
}
void c_R104::mark(){
	c_MethodInfo::mark();
}
c_R105::c_R105(){
}
c_R105* c_R105::m_new(){
	c_MethodInfo::m_new(String(L"ToArray",7),0,bb_reflection__classes[98],Array<c_ClassInfo* >());
	return this;
}
void c_R105::mark(){
	c_MethodInfo::mark();
}
c_R106::c_R106(){
}
c_R106* c_R106::m_new(){
	c_MethodInfo::m_new(String(L"ObjectEnumerator",16),0,bb_reflection__classes[57],Array<c_ClassInfo* >());
	return this;
}
void c_R106::mark(){
	c_MethodInfo::mark();
}
c_R107::c_R107(){
}
c_R107* c_R107::m_new(){
	c_ClassInfo* t_[]={bb_reflection__intClass};
	c_MethodInfo::m_new(String(L"Get",3),0,bb_reflection__intClass,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R107::mark(){
	c_MethodInfo::mark();
}
c_R108::c_R108(){
}
c_R108* c_R108::m_new(){
	c_ClassInfo* t_[]={bb_reflection__intClass,bb_reflection__intClass};
	c_MethodInfo::m_new(String(L"Set",3),0,0,Array<c_ClassInfo* >(t_,2));
	return this;
}
void c_R108::mark(){
	c_MethodInfo::mark();
}
c_R109::c_R109(){
}
c_R109* c_R109::m_new(){
	c_ClassInfo* t_[]={bb_reflection__intClass};
	c_MethodInfo::m_new(String(L"PushFirst",9),0,0,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R109::mark(){
	c_MethodInfo::mark();
}
c_R110::c_R110(){
}
c_R110* c_R110::m_new(){
	c_ClassInfo* t_[]={bb_reflection__intClass};
	c_MethodInfo::m_new(String(L"PushLast",8),0,0,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R110::mark(){
	c_MethodInfo::mark();
}
c_R111::c_R111(){
}
c_R111* c_R111::m_new(){
	c_MethodInfo::m_new(String(L"PopFirst",8),0,bb_reflection__intClass,Array<c_ClassInfo* >());
	return this;
}
void c_R111::mark(){
	c_MethodInfo::mark();
}
c_R112::c_R112(){
}
c_R112* c_R112::m_new(){
	c_MethodInfo::m_new(String(L"PopLast",7),0,bb_reflection__intClass,Array<c_ClassInfo* >());
	return this;
}
void c_R112::mark(){
	c_MethodInfo::mark();
}
c_R113::c_R113(){
}
c_R113* c_R113::m_new(){
	c_MethodInfo::m_new(String(L"First",5),0,bb_reflection__intClass,Array<c_ClassInfo* >());
	return this;
}
void c_R113::mark(){
	c_MethodInfo::mark();
}
c_R114::c_R114(){
}
c_R114* c_R114::m_new(){
	c_MethodInfo::m_new(String(L"Last",4),0,bb_reflection__intClass,Array<c_ClassInfo* >());
	return this;
}
void c_R114::mark(){
	c_MethodInfo::mark();
}
c_R120::c_R120(){
}
c_R120* c_R120::m_new(){
	c_MethodInfo::m_new(String(L"Grow",4),0,0,Array<c_ClassInfo* >());
	return this;
}
void c_R120::mark(){
	c_MethodInfo::mark();
}
c_R100::c_R100(){
}
c_R100* c_R100::m_new(){
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[5],Array<c_ClassInfo* >());
	return this;
}
void c_R100::mark(){
	c_FunctionInfo::mark();
}
c_R101::c_R101(){
}
c_R101* c_R101::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[98]};
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[5],Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R101::mark(){
	c_FunctionInfo::mark();
}
c_R122::c_R122(){
}
c_R122* c_R122::m_new(){
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[6],Array<c_ClassInfo* >());
	return this;
}
void c_R122::mark(){
	c_FunctionInfo::mark();
}
c_R123::c_R123(){
}
c_R123* c_R123::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[98]};
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[6],Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R123::mark(){
	c_FunctionInfo::mark();
}
c_R140::c_R140(){
}
c_R140* c_R140::m_new(){
	c_GlobalInfo::m_new(String(L"NIL",3),2,bb_reflection__floatClass);
	return this;
}
void c_R140::mark(){
	c_GlobalInfo::mark();
}
c_R141::c_R141(){
}
c_R141* c_R141::m_new(){
	c_FieldInfo::m_new(String(L"_data",5),2,bb_reflection__classes[99]);
	return this;
}
void c_R141::mark(){
	c_FieldInfo::mark();
}
c_R142::c_R142(){
}
c_R142* c_R142::m_new(){
	c_FieldInfo::m_new(String(L"_capacity",9),2,bb_reflection__intClass);
	return this;
}
void c_R142::mark(){
	c_FieldInfo::mark();
}
c_R143::c_R143(){
}
c_R143* c_R143::m_new(){
	c_FieldInfo::m_new(String(L"_first",6),2,bb_reflection__intClass);
	return this;
}
void c_R143::mark(){
	c_FieldInfo::mark();
}
c_R144::c_R144(){
}
c_R144* c_R144::m_new(){
	c_FieldInfo::m_new(String(L"_last",5),2,bb_reflection__intClass);
	return this;
}
void c_R144::mark(){
	c_FieldInfo::mark();
}
c_R127::c_R127(){
}
c_R127* c_R127::m_new(){
	c_MethodInfo::m_new(String(L"Clear",5),0,0,Array<c_ClassInfo* >());
	return this;
}
void c_R127::mark(){
	c_MethodInfo::mark();
}
c_R128::c_R128(){
}
c_R128* c_R128::m_new(){
	c_MethodInfo::m_new(String(L"Length",6),0,bb_reflection__intClass,Array<c_ClassInfo* >());
	return this;
}
void c_R128::mark(){
	c_MethodInfo::mark();
}
c_R129::c_R129(){
}
c_R129* c_R129::m_new(){
	c_MethodInfo::m_new(String(L"IsEmpty",7),0,bb_reflection__boolClass,Array<c_ClassInfo* >());
	return this;
}
void c_R129::mark(){
	c_MethodInfo::mark();
}
c_R130::c_R130(){
}
c_R130* c_R130::m_new(){
	c_MethodInfo::m_new(String(L"ToArray",7),0,bb_reflection__classes[99],Array<c_ClassInfo* >());
	return this;
}
void c_R130::mark(){
	c_MethodInfo::mark();
}
c_R131::c_R131(){
}
c_R131* c_R131::m_new(){
	c_MethodInfo::m_new(String(L"ObjectEnumerator",16),0,bb_reflection__classes[58],Array<c_ClassInfo* >());
	return this;
}
void c_R131::mark(){
	c_MethodInfo::mark();
}
c_R132::c_R132(){
}
c_R132* c_R132::m_new(){
	c_ClassInfo* t_[]={bb_reflection__intClass};
	c_MethodInfo::m_new(String(L"Get",3),0,bb_reflection__floatClass,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R132::mark(){
	c_MethodInfo::mark();
}
c_R133::c_R133(){
}
c_R133* c_R133::m_new(){
	c_ClassInfo* t_[]={bb_reflection__intClass,bb_reflection__floatClass};
	c_MethodInfo::m_new(String(L"Set",3),0,0,Array<c_ClassInfo* >(t_,2));
	return this;
}
void c_R133::mark(){
	c_MethodInfo::mark();
}
c_R134::c_R134(){
}
c_R134* c_R134::m_new(){
	c_ClassInfo* t_[]={bb_reflection__floatClass};
	c_MethodInfo::m_new(String(L"PushFirst",9),0,0,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R134::mark(){
	c_MethodInfo::mark();
}
c_R135::c_R135(){
}
c_R135* c_R135::m_new(){
	c_ClassInfo* t_[]={bb_reflection__floatClass};
	c_MethodInfo::m_new(String(L"PushLast",8),0,0,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R135::mark(){
	c_MethodInfo::mark();
}
c_R136::c_R136(){
}
c_R136* c_R136::m_new(){
	c_MethodInfo::m_new(String(L"PopFirst",8),0,bb_reflection__floatClass,Array<c_ClassInfo* >());
	return this;
}
void c_R136::mark(){
	c_MethodInfo::mark();
}
c_R137::c_R137(){
}
c_R137* c_R137::m_new(){
	c_MethodInfo::m_new(String(L"PopLast",7),0,bb_reflection__floatClass,Array<c_ClassInfo* >());
	return this;
}
void c_R137::mark(){
	c_MethodInfo::mark();
}
c_R138::c_R138(){
}
c_R138* c_R138::m_new(){
	c_MethodInfo::m_new(String(L"First",5),0,bb_reflection__floatClass,Array<c_ClassInfo* >());
	return this;
}
void c_R138::mark(){
	c_MethodInfo::mark();
}
c_R139::c_R139(){
}
c_R139* c_R139::m_new(){
	c_MethodInfo::m_new(String(L"Last",4),0,bb_reflection__floatClass,Array<c_ClassInfo* >());
	return this;
}
void c_R139::mark(){
	c_MethodInfo::mark();
}
c_R145::c_R145(){
}
c_R145* c_R145::m_new(){
	c_MethodInfo::m_new(String(L"Grow",4),0,0,Array<c_ClassInfo* >());
	return this;
}
void c_R145::mark(){
	c_MethodInfo::mark();
}
c_R125::c_R125(){
}
c_R125* c_R125::m_new(){
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[7],Array<c_ClassInfo* >());
	return this;
}
void c_R125::mark(){
	c_FunctionInfo::mark();
}
c_R126::c_R126(){
}
c_R126* c_R126::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[99]};
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[7],Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R126::mark(){
	c_FunctionInfo::mark();
}
c_R147::c_R147(){
}
c_R147* c_R147::m_new(){
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[8],Array<c_ClassInfo* >());
	return this;
}
void c_R147::mark(){
	c_FunctionInfo::mark();
}
c_R148::c_R148(){
}
c_R148* c_R148::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[99]};
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[8],Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R148::mark(){
	c_FunctionInfo::mark();
}
c_R165::c_R165(){
}
c_R165* c_R165::m_new(){
	c_GlobalInfo::m_new(String(L"NIL",3),2,bb_reflection__stringClass);
	return this;
}
void c_R165::mark(){
	c_GlobalInfo::mark();
}
c_R166::c_R166(){
}
c_R166* c_R166::m_new(){
	c_FieldInfo::m_new(String(L"_data",5),2,bb_reflection__classes[100]);
	return this;
}
void c_R166::mark(){
	c_FieldInfo::mark();
}
c_R167::c_R167(){
}
c_R167* c_R167::m_new(){
	c_FieldInfo::m_new(String(L"_capacity",9),2,bb_reflection__intClass);
	return this;
}
void c_R167::mark(){
	c_FieldInfo::mark();
}
c_R168::c_R168(){
}
c_R168* c_R168::m_new(){
	c_FieldInfo::m_new(String(L"_first",6),2,bb_reflection__intClass);
	return this;
}
void c_R168::mark(){
	c_FieldInfo::mark();
}
c_R169::c_R169(){
}
c_R169* c_R169::m_new(){
	c_FieldInfo::m_new(String(L"_last",5),2,bb_reflection__intClass);
	return this;
}
void c_R169::mark(){
	c_FieldInfo::mark();
}
c_R152::c_R152(){
}
c_R152* c_R152::m_new(){
	c_MethodInfo::m_new(String(L"Clear",5),0,0,Array<c_ClassInfo* >());
	return this;
}
void c_R152::mark(){
	c_MethodInfo::mark();
}
c_R153::c_R153(){
}
c_R153* c_R153::m_new(){
	c_MethodInfo::m_new(String(L"Length",6),0,bb_reflection__intClass,Array<c_ClassInfo* >());
	return this;
}
void c_R153::mark(){
	c_MethodInfo::mark();
}
c_R154::c_R154(){
}
c_R154* c_R154::m_new(){
	c_MethodInfo::m_new(String(L"IsEmpty",7),0,bb_reflection__boolClass,Array<c_ClassInfo* >());
	return this;
}
void c_R154::mark(){
	c_MethodInfo::mark();
}
c_R155::c_R155(){
}
c_R155* c_R155::m_new(){
	c_MethodInfo::m_new(String(L"ToArray",7),0,bb_reflection__classes[100],Array<c_ClassInfo* >());
	return this;
}
void c_R155::mark(){
	c_MethodInfo::mark();
}
c_R156::c_R156(){
}
c_R156* c_R156::m_new(){
	c_MethodInfo::m_new(String(L"ObjectEnumerator",16),0,bb_reflection__classes[59],Array<c_ClassInfo* >());
	return this;
}
void c_R156::mark(){
	c_MethodInfo::mark();
}
c_R157::c_R157(){
}
c_R157* c_R157::m_new(){
	c_ClassInfo* t_[]={bb_reflection__intClass};
	c_MethodInfo::m_new(String(L"Get",3),0,bb_reflection__stringClass,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R157::mark(){
	c_MethodInfo::mark();
}
c_R158::c_R158(){
}
c_R158* c_R158::m_new(){
	c_ClassInfo* t_[]={bb_reflection__intClass,bb_reflection__stringClass};
	c_MethodInfo::m_new(String(L"Set",3),0,0,Array<c_ClassInfo* >(t_,2));
	return this;
}
void c_R158::mark(){
	c_MethodInfo::mark();
}
c_R159::c_R159(){
}
c_R159* c_R159::m_new(){
	c_ClassInfo* t_[]={bb_reflection__stringClass};
	c_MethodInfo::m_new(String(L"PushFirst",9),0,0,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R159::mark(){
	c_MethodInfo::mark();
}
c_R160::c_R160(){
}
c_R160* c_R160::m_new(){
	c_ClassInfo* t_[]={bb_reflection__stringClass};
	c_MethodInfo::m_new(String(L"PushLast",8),0,0,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R160::mark(){
	c_MethodInfo::mark();
}
c_R161::c_R161(){
}
c_R161* c_R161::m_new(){
	c_MethodInfo::m_new(String(L"PopFirst",8),0,bb_reflection__stringClass,Array<c_ClassInfo* >());
	return this;
}
void c_R161::mark(){
	c_MethodInfo::mark();
}
c_R162::c_R162(){
}
c_R162* c_R162::m_new(){
	c_MethodInfo::m_new(String(L"PopLast",7),0,bb_reflection__stringClass,Array<c_ClassInfo* >());
	return this;
}
void c_R162::mark(){
	c_MethodInfo::mark();
}
c_R163::c_R163(){
}
c_R163* c_R163::m_new(){
	c_MethodInfo::m_new(String(L"First",5),0,bb_reflection__stringClass,Array<c_ClassInfo* >());
	return this;
}
void c_R163::mark(){
	c_MethodInfo::mark();
}
c_R164::c_R164(){
}
c_R164* c_R164::m_new(){
	c_MethodInfo::m_new(String(L"Last",4),0,bb_reflection__stringClass,Array<c_ClassInfo* >());
	return this;
}
void c_R164::mark(){
	c_MethodInfo::mark();
}
c_R170::c_R170(){
}
c_R170* c_R170::m_new(){
	c_MethodInfo::m_new(String(L"Grow",4),0,0,Array<c_ClassInfo* >());
	return this;
}
void c_R170::mark(){
	c_MethodInfo::mark();
}
c_R150::c_R150(){
}
c_R150* c_R150::m_new(){
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[9],Array<c_ClassInfo* >());
	return this;
}
void c_R150::mark(){
	c_FunctionInfo::mark();
}
c_R151::c_R151(){
}
c_R151* c_R151::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[100]};
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[9],Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R151::mark(){
	c_FunctionInfo::mark();
}
c_R172::c_R172(){
}
c_R172* c_R172::m_new(){
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[10],Array<c_ClassInfo* >());
	return this;
}
void c_R172::mark(){
	c_FunctionInfo::mark();
}
c_R173::c_R173(){
}
c_R173* c_R173::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[100]};
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[10],Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R173::mark(){
	c_FunctionInfo::mark();
}
c_R208::c_R208(){
}
c_R208* c_R208::m_new(){
	c_FieldInfo::m_new(String(L"_head",5),2,bb_reflection__classes[14]);
	return this;
}
void c_R208::mark(){
	c_FieldInfo::mark();
}
c_R178::c_R178(){
}
c_R178* c_R178::m_new(){
	c_MethodInfo::m_new(String(L"ToArray",7),0,bb_reflection__classes[98],Array<c_ClassInfo* >());
	return this;
}
void c_R178::mark(){
	c_MethodInfo::mark();
}
c_R179::c_R179(){
}
c_R179* c_R179::m_new(){
	c_ClassInfo* t_[]={bb_reflection__intClass,bb_reflection__intClass};
	c_MethodInfo::m_new(String(L"Equals",6),0,bb_reflection__boolClass,Array<c_ClassInfo* >(t_,2));
	return this;
}
void c_R179::mark(){
	c_MethodInfo::mark();
}
c_R180::c_R180(){
}
c_R180* c_R180::m_new(){
	c_ClassInfo* t_[]={bb_reflection__intClass,bb_reflection__intClass};
	c_MethodInfo::m_new(String(L"Compare",7),0,bb_reflection__intClass,Array<c_ClassInfo* >(t_,2));
	return this;
}
void c_R180::mark(){
	c_MethodInfo::mark();
}
c_R181::c_R181(){
}
c_R181* c_R181::m_new(){
	c_MethodInfo::m_new(String(L"Clear",5),0,bb_reflection__intClass,Array<c_ClassInfo* >());
	return this;
}
void c_R181::mark(){
	c_MethodInfo::mark();
}
c_R182::c_R182(){
}
c_R182* c_R182::m_new(){
	c_MethodInfo::m_new(String(L"Count",5),0,bb_reflection__intClass,Array<c_ClassInfo* >());
	return this;
}
void c_R182::mark(){
	c_MethodInfo::mark();
}
c_R183::c_R183(){
}
c_R183* c_R183::m_new(){
	c_MethodInfo::m_new(String(L"IsEmpty",7),0,bb_reflection__boolClass,Array<c_ClassInfo* >());
	return this;
}
void c_R183::mark(){
	c_MethodInfo::mark();
}
c_R184::c_R184(){
}
c_R184* c_R184::m_new(){
	c_ClassInfo* t_[]={bb_reflection__intClass};
	c_MethodInfo::m_new(String(L"Contains",8),0,bb_reflection__boolClass,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R184::mark(){
	c_MethodInfo::mark();
}
c_R185::c_R185(){
}
c_R185* c_R185::m_new(){
	c_MethodInfo::m_new(String(L"FirstNode",9),0,bb_reflection__classes[14],Array<c_ClassInfo* >());
	return this;
}
void c_R185::mark(){
	c_MethodInfo::mark();
}
c_R186::c_R186(){
}
c_R186* c_R186::m_new(){
	c_MethodInfo::m_new(String(L"LastNode",8),0,bb_reflection__classes[14],Array<c_ClassInfo* >());
	return this;
}
void c_R186::mark(){
	c_MethodInfo::mark();
}
c_R187::c_R187(){
}
c_R187* c_R187::m_new(){
	c_MethodInfo::m_new(String(L"First",5),0,bb_reflection__intClass,Array<c_ClassInfo* >());
	return this;
}
void c_R187::mark(){
	c_MethodInfo::mark();
}
c_R188::c_R188(){
}
c_R188* c_R188::m_new(){
	c_MethodInfo::m_new(String(L"Last",4),0,bb_reflection__intClass,Array<c_ClassInfo* >());
	return this;
}
void c_R188::mark(){
	c_MethodInfo::mark();
}
c_R189::c_R189(){
}
c_R189* c_R189::m_new(){
	c_MethodInfo::m_new(String(L"RemoveFirst",11),0,bb_reflection__intClass,Array<c_ClassInfo* >());
	return this;
}
void c_R189::mark(){
	c_MethodInfo::mark();
}
c_R190::c_R190(){
}
c_R190* c_R190::m_new(){
	c_MethodInfo::m_new(String(L"RemoveLast",10),0,bb_reflection__intClass,Array<c_ClassInfo* >());
	return this;
}
void c_R190::mark(){
	c_MethodInfo::mark();
}
c_R191::c_R191(){
}
c_R191* c_R191::m_new(){
	c_ClassInfo* t_[]={bb_reflection__intClass};
	c_MethodInfo::m_new(String(L"AddFirst",8),0,bb_reflection__classes[14],Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R191::mark(){
	c_MethodInfo::mark();
}
c_R192::c_R192(){
}
c_R192* c_R192::m_new(){
	c_ClassInfo* t_[]={bb_reflection__intClass};
	c_MethodInfo::m_new(String(L"AddLast",7),0,bb_reflection__classes[14],Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R192::mark(){
	c_MethodInfo::mark();
}
c_R193::c_R193(){
}
c_R193* c_R193::m_new(){
	c_ClassInfo* t_[]={bb_reflection__intClass};
	c_MethodInfo::m_new(String(L"Find",4),0,bb_reflection__classes[14],Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R193::mark(){
	c_MethodInfo::mark();
}
c_R194::c_R194(){
}
c_R194* c_R194::m_new(){
	c_ClassInfo* t_[]={bb_reflection__intClass,bb_reflection__classes[14]};
	c_MethodInfo::m_new(String(L"Find",4),0,bb_reflection__classes[14],Array<c_ClassInfo* >(t_,2));
	return this;
}
void c_R194::mark(){
	c_MethodInfo::mark();
}
c_R195::c_R195(){
}
c_R195* c_R195::m_new(){
	c_ClassInfo* t_[]={bb_reflection__intClass};
	c_MethodInfo::m_new(String(L"FindLast",8),0,bb_reflection__classes[14],Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R195::mark(){
	c_MethodInfo::mark();
}
c_R196::c_R196(){
}
c_R196* c_R196::m_new(){
	c_ClassInfo* t_[]={bb_reflection__intClass,bb_reflection__classes[14]};
	c_MethodInfo::m_new(String(L"FindLast",8),0,bb_reflection__classes[14],Array<c_ClassInfo* >(t_,2));
	return this;
}
void c_R196::mark(){
	c_MethodInfo::mark();
}
c_R197::c_R197(){
}
c_R197* c_R197::m_new(){
	c_ClassInfo* t_[]={bb_reflection__intClass};
	c_MethodInfo::m_new(String(L"Remove",6),0,0,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R197::mark(){
	c_MethodInfo::mark();
}
c_R198::c_R198(){
}
c_R198* c_R198::m_new(){
	c_ClassInfo* t_[]={bb_reflection__intClass};
	c_MethodInfo::m_new(String(L"RemoveFirst",11),0,0,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R198::mark(){
	c_MethodInfo::mark();
}
c_R199::c_R199(){
}
c_R199* c_R199::m_new(){
	c_ClassInfo* t_[]={bb_reflection__intClass};
	c_MethodInfo::m_new(String(L"RemoveLast",10),0,0,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R199::mark(){
	c_MethodInfo::mark();
}
c_R200::c_R200(){
}
c_R200* c_R200::m_new(){
	c_ClassInfo* t_[]={bb_reflection__intClass};
	c_MethodInfo::m_new(String(L"RemoveEach",10),0,bb_reflection__intClass,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R200::mark(){
	c_MethodInfo::mark();
}
c_R201::c_R201(){
}
c_R201* c_R201::m_new(){
	c_ClassInfo* t_[]={bb_reflection__intClass,bb_reflection__intClass};
	c_MethodInfo::m_new(String(L"InsertBefore",12),0,bb_reflection__classes[14],Array<c_ClassInfo* >(t_,2));
	return this;
}
void c_R201::mark(){
	c_MethodInfo::mark();
}
c_R202::c_R202(){
}
c_R202* c_R202::m_new(){
	c_ClassInfo* t_[]={bb_reflection__intClass,bb_reflection__intClass};
	c_MethodInfo::m_new(String(L"InsertAfter",11),0,bb_reflection__classes[14],Array<c_ClassInfo* >(t_,2));
	return this;
}
void c_R202::mark(){
	c_MethodInfo::mark();
}
c_R203::c_R203(){
}
c_R203* c_R203::m_new(){
	c_ClassInfo* t_[]={bb_reflection__intClass,bb_reflection__intClass};
	c_MethodInfo::m_new(String(L"InsertBeforeEach",16),0,0,Array<c_ClassInfo* >(t_,2));
	return this;
}
void c_R203::mark(){
	c_MethodInfo::mark();
}
c_R204::c_R204(){
}
c_R204* c_R204::m_new(){
	c_ClassInfo* t_[]={bb_reflection__intClass,bb_reflection__intClass};
	c_MethodInfo::m_new(String(L"InsertAfterEach",15),0,0,Array<c_ClassInfo* >(t_,2));
	return this;
}
void c_R204::mark(){
	c_MethodInfo::mark();
}
c_R205::c_R205(){
}
c_R205* c_R205::m_new(){
	c_MethodInfo::m_new(String(L"ObjectEnumerator",16),0,bb_reflection__classes[60],Array<c_ClassInfo* >());
	return this;
}
void c_R205::mark(){
	c_MethodInfo::mark();
}
c_R206::c_R206(){
}
c_R206* c_R206::m_new(){
	c_MethodInfo::m_new(String(L"Backwards",9),0,bb_reflection__classes[61],Array<c_ClassInfo* >());
	return this;
}
void c_R206::mark(){
	c_MethodInfo::mark();
}
c_R207::c_R207(){
}
c_R207* c_R207::m_new(){
	c_ClassInfo* t_[]={bb_reflection__intClass};
	c_MethodInfo::m_new(String(L"Sort",4),0,bb_reflection__intClass,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R207::mark(){
	c_MethodInfo::mark();
}
c_R176::c_R176(){
}
c_R176* c_R176::m_new(){
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[12],Array<c_ClassInfo* >());
	return this;
}
void c_R176::mark(){
	c_FunctionInfo::mark();
}
c_R177::c_R177(){
}
c_R177* c_R177::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[98]};
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[12],Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R177::mark(){
	c_FunctionInfo::mark();
}
c_R211::c_R211(){
}
c_R211* c_R211::m_new(){
	c_ClassInfo* t_[]={bb_reflection__intClass,bb_reflection__intClass};
	c_MethodInfo::m_new(String(L"Equals",6),0,bb_reflection__boolClass,Array<c_ClassInfo* >(t_,2));
	return this;
}
void c_R211::mark(){
	c_MethodInfo::mark();
}
c_R212::c_R212(){
}
c_R212* c_R212::m_new(){
	c_ClassInfo* t_[]={bb_reflection__intClass,bb_reflection__intClass};
	c_MethodInfo::m_new(String(L"Compare",7),0,bb_reflection__intClass,Array<c_ClassInfo* >(t_,2));
	return this;
}
void c_R212::mark(){
	c_MethodInfo::mark();
}
c_R210::c_R210(){
}
c_R210* c_R210::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[98]};
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[13],Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R210::mark(){
	c_FunctionInfo::mark();
}
c_R213::c_R213(){
}
c_R213* c_R213::m_new(){
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[13],Array<c_ClassInfo* >());
	return this;
}
void c_R213::mark(){
	c_FunctionInfo::mark();
}
c_R220::c_R220(){
}
c_R220* c_R220::m_new(){
	c_FieldInfo::m_new(String(L"_succ",5),2,bb_reflection__classes[14]);
	return this;
}
void c_R220::mark(){
	c_FieldInfo::mark();
}
c_R221::c_R221(){
}
c_R221* c_R221::m_new(){
	c_FieldInfo::m_new(String(L"_pred",5),2,bb_reflection__classes[14]);
	return this;
}
void c_R221::mark(){
	c_FieldInfo::mark();
}
c_R222::c_R222(){
}
c_R222* c_R222::m_new(){
	c_FieldInfo::m_new(String(L"_data",5),2,bb_reflection__intClass);
	return this;
}
void c_R222::mark(){
	c_FieldInfo::mark();
}
c_R216::c_R216(){
}
c_R216* c_R216::m_new(){
	c_MethodInfo::m_new(String(L"Value",5),0,bb_reflection__intClass,Array<c_ClassInfo* >());
	return this;
}
void c_R216::mark(){
	c_MethodInfo::mark();
}
c_R217::c_R217(){
}
c_R217* c_R217::m_new(){
	c_MethodInfo::m_new(String(L"Remove",6),0,bb_reflection__intClass,Array<c_ClassInfo* >());
	return this;
}
void c_R217::mark(){
	c_MethodInfo::mark();
}
c_R218::c_R218(){
}
c_R218* c_R218::m_new(){
	c_MethodInfo::m_new(String(L"NextNode",8),0,bb_reflection__classes[14],Array<c_ClassInfo* >());
	return this;
}
void c_R218::mark(){
	c_MethodInfo::mark();
}
c_R219::c_R219(){
}
c_R219* c_R219::m_new(){
	c_MethodInfo::m_new(String(L"PrevNode",8),0,bb_reflection__classes[14],Array<c_ClassInfo* >());
	return this;
}
void c_R219::mark(){
	c_MethodInfo::mark();
}
c_R223::c_R223(){
}
c_R223* c_R223::m_new(){
	c_MethodInfo::m_new(String(L"GetNode",7),0,bb_reflection__classes[14],Array<c_ClassInfo* >());
	return this;
}
void c_R223::mark(){
	c_MethodInfo::mark();
}
c_R215::c_R215(){
}
c_R215* c_R215::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[14],bb_reflection__classes[14],bb_reflection__intClass};
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[14],Array<c_ClassInfo* >(t_,3));
	return this;
}
void c_R215::mark(){
	c_FunctionInfo::mark();
}
c_R224::c_R224(){
}
c_R224* c_R224::m_new(){
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[14],Array<c_ClassInfo* >());
	return this;
}
void c_R224::mark(){
	c_FunctionInfo::mark();
}
c_R227::c_R227(){
}
c_R227* c_R227::m_new(){
	c_MethodInfo::m_new(String(L"GetNode",7),0,bb_reflection__classes[14],Array<c_ClassInfo* >());
	return this;
}
void c_R227::mark(){
	c_MethodInfo::mark();
}
c_R226::c_R226(){
}
c_R226* c_R226::m_new(){
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[15],Array<c_ClassInfo* >());
	return this;
}
void c_R226::mark(){
	c_FunctionInfo::mark();
}
c_R261::c_R261(){
}
c_R261* c_R261::m_new(){
	c_FieldInfo::m_new(String(L"_head",5),2,bb_reflection__classes[18]);
	return this;
}
void c_R261::mark(){
	c_FieldInfo::mark();
}
c_R231::c_R231(){
}
c_R231* c_R231::m_new(){
	c_MethodInfo::m_new(String(L"ToArray",7),0,bb_reflection__classes[99],Array<c_ClassInfo* >());
	return this;
}
void c_R231::mark(){
	c_MethodInfo::mark();
}
c_R232::c_R232(){
}
c_R232* c_R232::m_new(){
	c_ClassInfo* t_[]={bb_reflection__floatClass,bb_reflection__floatClass};
	c_MethodInfo::m_new(String(L"Equals",6),0,bb_reflection__boolClass,Array<c_ClassInfo* >(t_,2));
	return this;
}
void c_R232::mark(){
	c_MethodInfo::mark();
}
c_R233::c_R233(){
}
c_R233* c_R233::m_new(){
	c_ClassInfo* t_[]={bb_reflection__floatClass,bb_reflection__floatClass};
	c_MethodInfo::m_new(String(L"Compare",7),0,bb_reflection__intClass,Array<c_ClassInfo* >(t_,2));
	return this;
}
void c_R233::mark(){
	c_MethodInfo::mark();
}
c_R234::c_R234(){
}
c_R234* c_R234::m_new(){
	c_MethodInfo::m_new(String(L"Clear",5),0,bb_reflection__intClass,Array<c_ClassInfo* >());
	return this;
}
void c_R234::mark(){
	c_MethodInfo::mark();
}
c_R235::c_R235(){
}
c_R235* c_R235::m_new(){
	c_MethodInfo::m_new(String(L"Count",5),0,bb_reflection__intClass,Array<c_ClassInfo* >());
	return this;
}
void c_R235::mark(){
	c_MethodInfo::mark();
}
c_R236::c_R236(){
}
c_R236* c_R236::m_new(){
	c_MethodInfo::m_new(String(L"IsEmpty",7),0,bb_reflection__boolClass,Array<c_ClassInfo* >());
	return this;
}
void c_R236::mark(){
	c_MethodInfo::mark();
}
c_R237::c_R237(){
}
c_R237* c_R237::m_new(){
	c_ClassInfo* t_[]={bb_reflection__floatClass};
	c_MethodInfo::m_new(String(L"Contains",8),0,bb_reflection__boolClass,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R237::mark(){
	c_MethodInfo::mark();
}
c_R238::c_R238(){
}
c_R238* c_R238::m_new(){
	c_MethodInfo::m_new(String(L"FirstNode",9),0,bb_reflection__classes[18],Array<c_ClassInfo* >());
	return this;
}
void c_R238::mark(){
	c_MethodInfo::mark();
}
c_R239::c_R239(){
}
c_R239* c_R239::m_new(){
	c_MethodInfo::m_new(String(L"LastNode",8),0,bb_reflection__classes[18],Array<c_ClassInfo* >());
	return this;
}
void c_R239::mark(){
	c_MethodInfo::mark();
}
c_R240::c_R240(){
}
c_R240* c_R240::m_new(){
	c_MethodInfo::m_new(String(L"First",5),0,bb_reflection__floatClass,Array<c_ClassInfo* >());
	return this;
}
void c_R240::mark(){
	c_MethodInfo::mark();
}
c_R241::c_R241(){
}
c_R241* c_R241::m_new(){
	c_MethodInfo::m_new(String(L"Last",4),0,bb_reflection__floatClass,Array<c_ClassInfo* >());
	return this;
}
void c_R241::mark(){
	c_MethodInfo::mark();
}
c_R242::c_R242(){
}
c_R242* c_R242::m_new(){
	c_MethodInfo::m_new(String(L"RemoveFirst",11),0,bb_reflection__floatClass,Array<c_ClassInfo* >());
	return this;
}
void c_R242::mark(){
	c_MethodInfo::mark();
}
c_R243::c_R243(){
}
c_R243* c_R243::m_new(){
	c_MethodInfo::m_new(String(L"RemoveLast",10),0,bb_reflection__floatClass,Array<c_ClassInfo* >());
	return this;
}
void c_R243::mark(){
	c_MethodInfo::mark();
}
c_R244::c_R244(){
}
c_R244* c_R244::m_new(){
	c_ClassInfo* t_[]={bb_reflection__floatClass};
	c_MethodInfo::m_new(String(L"AddFirst",8),0,bb_reflection__classes[18],Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R244::mark(){
	c_MethodInfo::mark();
}
c_R245::c_R245(){
}
c_R245* c_R245::m_new(){
	c_ClassInfo* t_[]={bb_reflection__floatClass};
	c_MethodInfo::m_new(String(L"AddLast",7),0,bb_reflection__classes[18],Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R245::mark(){
	c_MethodInfo::mark();
}
c_R246::c_R246(){
}
c_R246* c_R246::m_new(){
	c_ClassInfo* t_[]={bb_reflection__floatClass};
	c_MethodInfo::m_new(String(L"Find",4),0,bb_reflection__classes[18],Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R246::mark(){
	c_MethodInfo::mark();
}
c_R247::c_R247(){
}
c_R247* c_R247::m_new(){
	c_ClassInfo* t_[]={bb_reflection__floatClass,bb_reflection__classes[18]};
	c_MethodInfo::m_new(String(L"Find",4),0,bb_reflection__classes[18],Array<c_ClassInfo* >(t_,2));
	return this;
}
void c_R247::mark(){
	c_MethodInfo::mark();
}
c_R248::c_R248(){
}
c_R248* c_R248::m_new(){
	c_ClassInfo* t_[]={bb_reflection__floatClass};
	c_MethodInfo::m_new(String(L"FindLast",8),0,bb_reflection__classes[18],Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R248::mark(){
	c_MethodInfo::mark();
}
c_R249::c_R249(){
}
c_R249* c_R249::m_new(){
	c_ClassInfo* t_[]={bb_reflection__floatClass,bb_reflection__classes[18]};
	c_MethodInfo::m_new(String(L"FindLast",8),0,bb_reflection__classes[18],Array<c_ClassInfo* >(t_,2));
	return this;
}
void c_R249::mark(){
	c_MethodInfo::mark();
}
c_R250::c_R250(){
}
c_R250* c_R250::m_new(){
	c_ClassInfo* t_[]={bb_reflection__floatClass};
	c_MethodInfo::m_new(String(L"Remove",6),0,0,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R250::mark(){
	c_MethodInfo::mark();
}
c_R251::c_R251(){
}
c_R251* c_R251::m_new(){
	c_ClassInfo* t_[]={bb_reflection__floatClass};
	c_MethodInfo::m_new(String(L"RemoveFirst",11),0,0,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R251::mark(){
	c_MethodInfo::mark();
}
c_R252::c_R252(){
}
c_R252* c_R252::m_new(){
	c_ClassInfo* t_[]={bb_reflection__floatClass};
	c_MethodInfo::m_new(String(L"RemoveLast",10),0,0,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R252::mark(){
	c_MethodInfo::mark();
}
c_R253::c_R253(){
}
c_R253* c_R253::m_new(){
	c_ClassInfo* t_[]={bb_reflection__floatClass};
	c_MethodInfo::m_new(String(L"RemoveEach",10),0,bb_reflection__intClass,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R253::mark(){
	c_MethodInfo::mark();
}
c_R254::c_R254(){
}
c_R254* c_R254::m_new(){
	c_ClassInfo* t_[]={bb_reflection__floatClass,bb_reflection__floatClass};
	c_MethodInfo::m_new(String(L"InsertBefore",12),0,bb_reflection__classes[18],Array<c_ClassInfo* >(t_,2));
	return this;
}
void c_R254::mark(){
	c_MethodInfo::mark();
}
c_R255::c_R255(){
}
c_R255* c_R255::m_new(){
	c_ClassInfo* t_[]={bb_reflection__floatClass,bb_reflection__floatClass};
	c_MethodInfo::m_new(String(L"InsertAfter",11),0,bb_reflection__classes[18],Array<c_ClassInfo* >(t_,2));
	return this;
}
void c_R255::mark(){
	c_MethodInfo::mark();
}
c_R256::c_R256(){
}
c_R256* c_R256::m_new(){
	c_ClassInfo* t_[]={bb_reflection__floatClass,bb_reflection__floatClass};
	c_MethodInfo::m_new(String(L"InsertBeforeEach",16),0,0,Array<c_ClassInfo* >(t_,2));
	return this;
}
void c_R256::mark(){
	c_MethodInfo::mark();
}
c_R257::c_R257(){
}
c_R257* c_R257::m_new(){
	c_ClassInfo* t_[]={bb_reflection__floatClass,bb_reflection__floatClass};
	c_MethodInfo::m_new(String(L"InsertAfterEach",15),0,0,Array<c_ClassInfo* >(t_,2));
	return this;
}
void c_R257::mark(){
	c_MethodInfo::mark();
}
c_R258::c_R258(){
}
c_R258* c_R258::m_new(){
	c_MethodInfo::m_new(String(L"ObjectEnumerator",16),0,bb_reflection__classes[62],Array<c_ClassInfo* >());
	return this;
}
void c_R258::mark(){
	c_MethodInfo::mark();
}
c_R259::c_R259(){
}
c_R259* c_R259::m_new(){
	c_MethodInfo::m_new(String(L"Backwards",9),0,bb_reflection__classes[63],Array<c_ClassInfo* >());
	return this;
}
void c_R259::mark(){
	c_MethodInfo::mark();
}
c_R260::c_R260(){
}
c_R260* c_R260::m_new(){
	c_ClassInfo* t_[]={bb_reflection__intClass};
	c_MethodInfo::m_new(String(L"Sort",4),0,bb_reflection__intClass,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R260::mark(){
	c_MethodInfo::mark();
}
c_R229::c_R229(){
}
c_R229* c_R229::m_new(){
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[16],Array<c_ClassInfo* >());
	return this;
}
void c_R229::mark(){
	c_FunctionInfo::mark();
}
c_R230::c_R230(){
}
c_R230* c_R230::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[99]};
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[16],Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R230::mark(){
	c_FunctionInfo::mark();
}
c_R264::c_R264(){
}
c_R264* c_R264::m_new(){
	c_ClassInfo* t_[]={bb_reflection__floatClass,bb_reflection__floatClass};
	c_MethodInfo::m_new(String(L"Equals",6),0,bb_reflection__boolClass,Array<c_ClassInfo* >(t_,2));
	return this;
}
void c_R264::mark(){
	c_MethodInfo::mark();
}
c_R265::c_R265(){
}
c_R265* c_R265::m_new(){
	c_ClassInfo* t_[]={bb_reflection__floatClass,bb_reflection__floatClass};
	c_MethodInfo::m_new(String(L"Compare",7),0,bb_reflection__intClass,Array<c_ClassInfo* >(t_,2));
	return this;
}
void c_R265::mark(){
	c_MethodInfo::mark();
}
c_R263::c_R263(){
}
c_R263* c_R263::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[99]};
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[17],Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R263::mark(){
	c_FunctionInfo::mark();
}
c_R266::c_R266(){
}
c_R266* c_R266::m_new(){
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[17],Array<c_ClassInfo* >());
	return this;
}
void c_R266::mark(){
	c_FunctionInfo::mark();
}
c_R273::c_R273(){
}
c_R273* c_R273::m_new(){
	c_FieldInfo::m_new(String(L"_succ",5),2,bb_reflection__classes[18]);
	return this;
}
void c_R273::mark(){
	c_FieldInfo::mark();
}
c_R274::c_R274(){
}
c_R274* c_R274::m_new(){
	c_FieldInfo::m_new(String(L"_pred",5),2,bb_reflection__classes[18]);
	return this;
}
void c_R274::mark(){
	c_FieldInfo::mark();
}
c_R275::c_R275(){
}
c_R275* c_R275::m_new(){
	c_FieldInfo::m_new(String(L"_data",5),2,bb_reflection__floatClass);
	return this;
}
void c_R275::mark(){
	c_FieldInfo::mark();
}
c_R269::c_R269(){
}
c_R269* c_R269::m_new(){
	c_MethodInfo::m_new(String(L"Value",5),0,bb_reflection__floatClass,Array<c_ClassInfo* >());
	return this;
}
void c_R269::mark(){
	c_MethodInfo::mark();
}
c_R270::c_R270(){
}
c_R270* c_R270::m_new(){
	c_MethodInfo::m_new(String(L"Remove",6),0,bb_reflection__intClass,Array<c_ClassInfo* >());
	return this;
}
void c_R270::mark(){
	c_MethodInfo::mark();
}
c_R271::c_R271(){
}
c_R271* c_R271::m_new(){
	c_MethodInfo::m_new(String(L"NextNode",8),0,bb_reflection__classes[18],Array<c_ClassInfo* >());
	return this;
}
void c_R271::mark(){
	c_MethodInfo::mark();
}
c_R272::c_R272(){
}
c_R272* c_R272::m_new(){
	c_MethodInfo::m_new(String(L"PrevNode",8),0,bb_reflection__classes[18],Array<c_ClassInfo* >());
	return this;
}
void c_R272::mark(){
	c_MethodInfo::mark();
}
c_R276::c_R276(){
}
c_R276* c_R276::m_new(){
	c_MethodInfo::m_new(String(L"GetNode",7),0,bb_reflection__classes[18],Array<c_ClassInfo* >());
	return this;
}
void c_R276::mark(){
	c_MethodInfo::mark();
}
c_R268::c_R268(){
}
c_R268* c_R268::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[18],bb_reflection__classes[18],bb_reflection__floatClass};
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[18],Array<c_ClassInfo* >(t_,3));
	return this;
}
void c_R268::mark(){
	c_FunctionInfo::mark();
}
c_R277::c_R277(){
}
c_R277* c_R277::m_new(){
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[18],Array<c_ClassInfo* >());
	return this;
}
void c_R277::mark(){
	c_FunctionInfo::mark();
}
c_R280::c_R280(){
}
c_R280* c_R280::m_new(){
	c_MethodInfo::m_new(String(L"GetNode",7),0,bb_reflection__classes[18],Array<c_ClassInfo* >());
	return this;
}
void c_R280::mark(){
	c_MethodInfo::mark();
}
c_R279::c_R279(){
}
c_R279* c_R279::m_new(){
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[19],Array<c_ClassInfo* >());
	return this;
}
void c_R279::mark(){
	c_FunctionInfo::mark();
}
c_R314::c_R314(){
}
c_R314* c_R314::m_new(){
	c_FieldInfo::m_new(String(L"_head",5),2,bb_reflection__classes[22]);
	return this;
}
void c_R314::mark(){
	c_FieldInfo::mark();
}
c_R284::c_R284(){
}
c_R284* c_R284::m_new(){
	c_MethodInfo::m_new(String(L"ToArray",7),0,bb_reflection__classes[100],Array<c_ClassInfo* >());
	return this;
}
void c_R284::mark(){
	c_MethodInfo::mark();
}
c_R285::c_R285(){
}
c_R285* c_R285::m_new(){
	c_ClassInfo* t_[]={bb_reflection__stringClass,bb_reflection__stringClass};
	c_MethodInfo::m_new(String(L"Equals",6),0,bb_reflection__boolClass,Array<c_ClassInfo* >(t_,2));
	return this;
}
void c_R285::mark(){
	c_MethodInfo::mark();
}
c_R286::c_R286(){
}
c_R286* c_R286::m_new(){
	c_ClassInfo* t_[]={bb_reflection__stringClass,bb_reflection__stringClass};
	c_MethodInfo::m_new(String(L"Compare",7),0,bb_reflection__intClass,Array<c_ClassInfo* >(t_,2));
	return this;
}
void c_R286::mark(){
	c_MethodInfo::mark();
}
c_R287::c_R287(){
}
c_R287* c_R287::m_new(){
	c_MethodInfo::m_new(String(L"Clear",5),0,bb_reflection__intClass,Array<c_ClassInfo* >());
	return this;
}
void c_R287::mark(){
	c_MethodInfo::mark();
}
c_R288::c_R288(){
}
c_R288* c_R288::m_new(){
	c_MethodInfo::m_new(String(L"Count",5),0,bb_reflection__intClass,Array<c_ClassInfo* >());
	return this;
}
void c_R288::mark(){
	c_MethodInfo::mark();
}
c_R289::c_R289(){
}
c_R289* c_R289::m_new(){
	c_MethodInfo::m_new(String(L"IsEmpty",7),0,bb_reflection__boolClass,Array<c_ClassInfo* >());
	return this;
}
void c_R289::mark(){
	c_MethodInfo::mark();
}
c_R290::c_R290(){
}
c_R290* c_R290::m_new(){
	c_ClassInfo* t_[]={bb_reflection__stringClass};
	c_MethodInfo::m_new(String(L"Contains",8),0,bb_reflection__boolClass,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R290::mark(){
	c_MethodInfo::mark();
}
c_R291::c_R291(){
}
c_R291* c_R291::m_new(){
	c_MethodInfo::m_new(String(L"FirstNode",9),0,bb_reflection__classes[22],Array<c_ClassInfo* >());
	return this;
}
void c_R291::mark(){
	c_MethodInfo::mark();
}
c_R292::c_R292(){
}
c_R292* c_R292::m_new(){
	c_MethodInfo::m_new(String(L"LastNode",8),0,bb_reflection__classes[22],Array<c_ClassInfo* >());
	return this;
}
void c_R292::mark(){
	c_MethodInfo::mark();
}
c_R293::c_R293(){
}
c_R293* c_R293::m_new(){
	c_MethodInfo::m_new(String(L"First",5),0,bb_reflection__stringClass,Array<c_ClassInfo* >());
	return this;
}
void c_R293::mark(){
	c_MethodInfo::mark();
}
c_R294::c_R294(){
}
c_R294* c_R294::m_new(){
	c_MethodInfo::m_new(String(L"Last",4),0,bb_reflection__stringClass,Array<c_ClassInfo* >());
	return this;
}
void c_R294::mark(){
	c_MethodInfo::mark();
}
c_R295::c_R295(){
}
c_R295* c_R295::m_new(){
	c_MethodInfo::m_new(String(L"RemoveFirst",11),0,bb_reflection__stringClass,Array<c_ClassInfo* >());
	return this;
}
void c_R295::mark(){
	c_MethodInfo::mark();
}
c_R296::c_R296(){
}
c_R296* c_R296::m_new(){
	c_MethodInfo::m_new(String(L"RemoveLast",10),0,bb_reflection__stringClass,Array<c_ClassInfo* >());
	return this;
}
void c_R296::mark(){
	c_MethodInfo::mark();
}
c_R297::c_R297(){
}
c_R297* c_R297::m_new(){
	c_ClassInfo* t_[]={bb_reflection__stringClass};
	c_MethodInfo::m_new(String(L"AddFirst",8),0,bb_reflection__classes[22],Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R297::mark(){
	c_MethodInfo::mark();
}
c_R298::c_R298(){
}
c_R298* c_R298::m_new(){
	c_ClassInfo* t_[]={bb_reflection__stringClass};
	c_MethodInfo::m_new(String(L"AddLast",7),0,bb_reflection__classes[22],Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R298::mark(){
	c_MethodInfo::mark();
}
c_R299::c_R299(){
}
c_R299* c_R299::m_new(){
	c_ClassInfo* t_[]={bb_reflection__stringClass};
	c_MethodInfo::m_new(String(L"Find",4),0,bb_reflection__classes[22],Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R299::mark(){
	c_MethodInfo::mark();
}
c_R300::c_R300(){
}
c_R300* c_R300::m_new(){
	c_ClassInfo* t_[]={bb_reflection__stringClass,bb_reflection__classes[22]};
	c_MethodInfo::m_new(String(L"Find",4),0,bb_reflection__classes[22],Array<c_ClassInfo* >(t_,2));
	return this;
}
void c_R300::mark(){
	c_MethodInfo::mark();
}
c_R301::c_R301(){
}
c_R301* c_R301::m_new(){
	c_ClassInfo* t_[]={bb_reflection__stringClass};
	c_MethodInfo::m_new(String(L"FindLast",8),0,bb_reflection__classes[22],Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R301::mark(){
	c_MethodInfo::mark();
}
c_R302::c_R302(){
}
c_R302* c_R302::m_new(){
	c_ClassInfo* t_[]={bb_reflection__stringClass,bb_reflection__classes[22]};
	c_MethodInfo::m_new(String(L"FindLast",8),0,bb_reflection__classes[22],Array<c_ClassInfo* >(t_,2));
	return this;
}
void c_R302::mark(){
	c_MethodInfo::mark();
}
c_R303::c_R303(){
}
c_R303* c_R303::m_new(){
	c_ClassInfo* t_[]={bb_reflection__stringClass};
	c_MethodInfo::m_new(String(L"Remove",6),0,0,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R303::mark(){
	c_MethodInfo::mark();
}
c_R304::c_R304(){
}
c_R304* c_R304::m_new(){
	c_ClassInfo* t_[]={bb_reflection__stringClass};
	c_MethodInfo::m_new(String(L"RemoveFirst",11),0,0,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R304::mark(){
	c_MethodInfo::mark();
}
c_R305::c_R305(){
}
c_R305* c_R305::m_new(){
	c_ClassInfo* t_[]={bb_reflection__stringClass};
	c_MethodInfo::m_new(String(L"RemoveLast",10),0,0,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R305::mark(){
	c_MethodInfo::mark();
}
c_R306::c_R306(){
}
c_R306* c_R306::m_new(){
	c_ClassInfo* t_[]={bb_reflection__stringClass};
	c_MethodInfo::m_new(String(L"RemoveEach",10),0,bb_reflection__intClass,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R306::mark(){
	c_MethodInfo::mark();
}
c_R307::c_R307(){
}
c_R307* c_R307::m_new(){
	c_ClassInfo* t_[]={bb_reflection__stringClass,bb_reflection__stringClass};
	c_MethodInfo::m_new(String(L"InsertBefore",12),0,bb_reflection__classes[22],Array<c_ClassInfo* >(t_,2));
	return this;
}
void c_R307::mark(){
	c_MethodInfo::mark();
}
c_R308::c_R308(){
}
c_R308* c_R308::m_new(){
	c_ClassInfo* t_[]={bb_reflection__stringClass,bb_reflection__stringClass};
	c_MethodInfo::m_new(String(L"InsertAfter",11),0,bb_reflection__classes[22],Array<c_ClassInfo* >(t_,2));
	return this;
}
void c_R308::mark(){
	c_MethodInfo::mark();
}
c_R309::c_R309(){
}
c_R309* c_R309::m_new(){
	c_ClassInfo* t_[]={bb_reflection__stringClass,bb_reflection__stringClass};
	c_MethodInfo::m_new(String(L"InsertBeforeEach",16),0,0,Array<c_ClassInfo* >(t_,2));
	return this;
}
void c_R309::mark(){
	c_MethodInfo::mark();
}
c_R310::c_R310(){
}
c_R310* c_R310::m_new(){
	c_ClassInfo* t_[]={bb_reflection__stringClass,bb_reflection__stringClass};
	c_MethodInfo::m_new(String(L"InsertAfterEach",15),0,0,Array<c_ClassInfo* >(t_,2));
	return this;
}
void c_R310::mark(){
	c_MethodInfo::mark();
}
c_R311::c_R311(){
}
c_R311* c_R311::m_new(){
	c_MethodInfo::m_new(String(L"ObjectEnumerator",16),0,bb_reflection__classes[24],Array<c_ClassInfo* >());
	return this;
}
void c_R311::mark(){
	c_MethodInfo::mark();
}
c_R312::c_R312(){
}
c_R312* c_R312::m_new(){
	c_MethodInfo::m_new(String(L"Backwards",9),0,bb_reflection__classes[64],Array<c_ClassInfo* >());
	return this;
}
void c_R312::mark(){
	c_MethodInfo::mark();
}
c_R313::c_R313(){
}
c_R313* c_R313::m_new(){
	c_ClassInfo* t_[]={bb_reflection__intClass};
	c_MethodInfo::m_new(String(L"Sort",4),0,bb_reflection__intClass,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R313::mark(){
	c_MethodInfo::mark();
}
c_R282::c_R282(){
}
c_R282* c_R282::m_new(){
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[20],Array<c_ClassInfo* >());
	return this;
}
void c_R282::mark(){
	c_FunctionInfo::mark();
}
c_R283::c_R283(){
}
c_R283* c_R283::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[100]};
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[20],Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R283::mark(){
	c_FunctionInfo::mark();
}
c_R317::c_R317(){
}
c_R317* c_R317::m_new(){
	c_ClassInfo* t_[]={bb_reflection__stringClass};
	c_MethodInfo::m_new(String(L"Join",4),0,bb_reflection__stringClass,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R317::mark(){
	c_MethodInfo::mark();
}
c_R318::c_R318(){
}
c_R318* c_R318::m_new(){
	c_ClassInfo* t_[]={bb_reflection__stringClass,bb_reflection__stringClass};
	c_MethodInfo::m_new(String(L"Equals",6),0,bb_reflection__boolClass,Array<c_ClassInfo* >(t_,2));
	return this;
}
void c_R318::mark(){
	c_MethodInfo::mark();
}
c_R319::c_R319(){
}
c_R319* c_R319::m_new(){
	c_ClassInfo* t_[]={bb_reflection__stringClass,bb_reflection__stringClass};
	c_MethodInfo::m_new(String(L"Compare",7),0,bb_reflection__intClass,Array<c_ClassInfo* >(t_,2));
	return this;
}
void c_R319::mark(){
	c_MethodInfo::mark();
}
c_R316::c_R316(){
}
c_R316* c_R316::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[100]};
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[21],Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R316::mark(){
	c_FunctionInfo::mark();
}
c_R320::c_R320(){
}
c_R320* c_R320::m_new(){
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[21],Array<c_ClassInfo* >());
	return this;
}
void c_R320::mark(){
	c_FunctionInfo::mark();
}
c_R327::c_R327(){
}
c_R327* c_R327::m_new(){
	c_FieldInfo::m_new(String(L"_succ",5),2,bb_reflection__classes[22]);
	return this;
}
void c_R327::mark(){
	c_FieldInfo::mark();
}
c_R328::c_R328(){
}
c_R328* c_R328::m_new(){
	c_FieldInfo::m_new(String(L"_pred",5),2,bb_reflection__classes[22]);
	return this;
}
void c_R328::mark(){
	c_FieldInfo::mark();
}
c_R329::c_R329(){
}
c_R329* c_R329::m_new(){
	c_FieldInfo::m_new(String(L"_data",5),2,bb_reflection__stringClass);
	return this;
}
void c_R329::mark(){
	c_FieldInfo::mark();
}
c_R323::c_R323(){
}
c_R323* c_R323::m_new(){
	c_MethodInfo::m_new(String(L"Value",5),0,bb_reflection__stringClass,Array<c_ClassInfo* >());
	return this;
}
void c_R323::mark(){
	c_MethodInfo::mark();
}
c_R324::c_R324(){
}
c_R324* c_R324::m_new(){
	c_MethodInfo::m_new(String(L"Remove",6),0,bb_reflection__intClass,Array<c_ClassInfo* >());
	return this;
}
void c_R324::mark(){
	c_MethodInfo::mark();
}
c_R325::c_R325(){
}
c_R325* c_R325::m_new(){
	c_MethodInfo::m_new(String(L"NextNode",8),0,bb_reflection__classes[22],Array<c_ClassInfo* >());
	return this;
}
void c_R325::mark(){
	c_MethodInfo::mark();
}
c_R326::c_R326(){
}
c_R326* c_R326::m_new(){
	c_MethodInfo::m_new(String(L"PrevNode",8),0,bb_reflection__classes[22],Array<c_ClassInfo* >());
	return this;
}
void c_R326::mark(){
	c_MethodInfo::mark();
}
c_R330::c_R330(){
}
c_R330* c_R330::m_new(){
	c_MethodInfo::m_new(String(L"GetNode",7),0,bb_reflection__classes[22],Array<c_ClassInfo* >());
	return this;
}
void c_R330::mark(){
	c_MethodInfo::mark();
}
c_R322::c_R322(){
}
c_R322* c_R322::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[22],bb_reflection__classes[22],bb_reflection__stringClass};
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[22],Array<c_ClassInfo* >(t_,3));
	return this;
}
void c_R322::mark(){
	c_FunctionInfo::mark();
}
c_R331::c_R331(){
}
c_R331* c_R331::m_new(){
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[22],Array<c_ClassInfo* >());
	return this;
}
void c_R331::mark(){
	c_FunctionInfo::mark();
}
c_R334::c_R334(){
}
c_R334* c_R334::m_new(){
	c_MethodInfo::m_new(String(L"GetNode",7),0,bb_reflection__classes[22],Array<c_ClassInfo* >());
	return this;
}
void c_R334::mark(){
	c_MethodInfo::mark();
}
c_R333::c_R333(){
}
c_R333* c_R333::m_new(){
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[23],Array<c_ClassInfo* >());
	return this;
}
void c_R333::mark(){
	c_FunctionInfo::mark();
}
c_R339::c_R339(){
}
c_R339* c_R339::m_new(){
	c_FieldInfo::m_new(String(L"_list",5),2,bb_reflection__classes[20]);
	return this;
}
void c_R339::mark(){
	c_FieldInfo::mark();
}
c_R340::c_R340(){
}
c_R340* c_R340::m_new(){
	c_FieldInfo::m_new(String(L"_curr",5),2,bb_reflection__classes[22]);
	return this;
}
void c_R340::mark(){
	c_FieldInfo::mark();
}
c_R337::c_R337(){
}
c_R337* c_R337::m_new(){
	c_MethodInfo::m_new(String(L"HasNext",7),0,bb_reflection__boolClass,Array<c_ClassInfo* >());
	return this;
}
void c_R337::mark(){
	c_MethodInfo::mark();
}
c_R338::c_R338(){
}
c_R338* c_R338::m_new(){
	c_MethodInfo::m_new(String(L"NextObject",10),0,bb_reflection__stringClass,Array<c_ClassInfo* >());
	return this;
}
void c_R338::mark(){
	c_MethodInfo::mark();
}
c_R336::c_R336(){
}
c_R336* c_R336::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[20]};
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[24],Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R336::mark(){
	c_FunctionInfo::mark();
}
c_R341::c_R341(){
}
c_R341* c_R341::m_new(){
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[24],Array<c_ClassInfo* >());
	return this;
}
void c_R341::mark(){
	c_FunctionInfo::mark();
}
c_R351::c_R351(){
}
c_R351* c_R351::m_new(){
	c_FieldInfo::m_new(String(L"map",3),2,bb_reflection__classes[27]);
	return this;
}
void c_R351::mark(){
	c_FieldInfo::mark();
}
c_R344::c_R344(){
}
c_R344* c_R344::m_new(){
	c_MethodInfo::m_new(String(L"Clear",5),0,bb_reflection__intClass,Array<c_ClassInfo* >());
	return this;
}
void c_R344::mark(){
	c_MethodInfo::mark();
}
c_R345::c_R345(){
}
c_R345* c_R345::m_new(){
	c_MethodInfo::m_new(String(L"Count",5),0,bb_reflection__intClass,Array<c_ClassInfo* >());
	return this;
}
void c_R345::mark(){
	c_MethodInfo::mark();
}
c_R346::c_R346(){
}
c_R346* c_R346::m_new(){
	c_MethodInfo::m_new(String(L"IsEmpty",7),0,bb_reflection__boolClass,Array<c_ClassInfo* >());
	return this;
}
void c_R346::mark(){
	c_MethodInfo::mark();
}
c_R347::c_R347(){
}
c_R347* c_R347::m_new(){
	c_ClassInfo* t_[]={bb_reflection__intClass};
	c_MethodInfo::m_new(String(L"Contains",8),0,bb_reflection__boolClass,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R347::mark(){
	c_MethodInfo::mark();
}
c_R348::c_R348(){
}
c_R348* c_R348::m_new(){
	c_ClassInfo* t_[]={bb_reflection__intClass};
	c_MethodInfo::m_new(String(L"Insert",6),0,bb_reflection__intClass,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R348::mark(){
	c_MethodInfo::mark();
}
c_R349::c_R349(){
}
c_R349* c_R349::m_new(){
	c_ClassInfo* t_[]={bb_reflection__intClass};
	c_MethodInfo::m_new(String(L"Remove",6),0,bb_reflection__intClass,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R349::mark(){
	c_MethodInfo::mark();
}
c_R350::c_R350(){
}
c_R350* c_R350::m_new(){
	c_MethodInfo::m_new(String(L"ObjectEnumerator",16),0,bb_reflection__classes[82],Array<c_ClassInfo* >());
	return this;
}
void c_R350::mark(){
	c_MethodInfo::mark();
}
c_R343::c_R343(){
}
c_R343* c_R343::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[27]};
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[25],Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R343::mark(){
	c_FunctionInfo::mark();
}
c_R352::c_R352(){
}
c_R352* c_R352::m_new(){
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[25],Array<c_ClassInfo* >());
	return this;
}
void c_R352::mark(){
	c_FunctionInfo::mark();
}
c_R354::c_R354(){
}
c_R354* c_R354::m_new(){
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[26],Array<c_ClassInfo* >());
	return this;
}
void c_R354::mark(){
	c_FunctionInfo::mark();
}
c_R379::c_R379(){
}
c_R379* c_R379::m_new(){
	c_FieldInfo::m_new(String(L"root",4),2,bb_reflection__classes[68]);
	return this;
}
void c_R379::mark(){
	c_FieldInfo::mark();
}
c_R356::c_R356(){
}
c_R356* c_R356::m_new(){
	c_ClassInfo* t_[]={bb_reflection__intClass,bb_reflection__intClass};
	c_MethodInfo::m_new(String(L"Compare",7),4,bb_reflection__intClass,Array<c_ClassInfo* >(t_,2));
	return this;
}
void c_R356::mark(){
	c_MethodInfo::mark();
}
c_R357::c_R357(){
}
c_R357* c_R357::m_new(){
	c_MethodInfo::m_new(String(L"Clear",5),0,bb_reflection__intClass,Array<c_ClassInfo* >());
	return this;
}
void c_R357::mark(){
	c_MethodInfo::mark();
}
c_R358::c_R358(){
}
c_R358* c_R358::m_new(){
	c_MethodInfo::m_new(String(L"Count",5),0,bb_reflection__intClass,Array<c_ClassInfo* >());
	return this;
}
void c_R358::mark(){
	c_MethodInfo::mark();
}
c_R359::c_R359(){
}
c_R359* c_R359::m_new(){
	c_MethodInfo::m_new(String(L"IsEmpty",7),0,bb_reflection__boolClass,Array<c_ClassInfo* >());
	return this;
}
void c_R359::mark(){
	c_MethodInfo::mark();
}
c_R360::c_R360(){
}
c_R360* c_R360::m_new(){
	c_ClassInfo* t_[]={bb_reflection__intClass};
	c_MethodInfo::m_new(String(L"Contains",8),0,bb_reflection__boolClass,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R360::mark(){
	c_MethodInfo::mark();
}
c_R361::c_R361(){
}
c_R361* c_R361::m_new(){
	c_ClassInfo* t_[]={bb_reflection__intClass,bb_reflection__classes[0]};
	c_MethodInfo::m_new(String(L"Set",3),0,bb_reflection__boolClass,Array<c_ClassInfo* >(t_,2));
	return this;
}
void c_R361::mark(){
	c_MethodInfo::mark();
}
c_R362::c_R362(){
}
c_R362* c_R362::m_new(){
	c_ClassInfo* t_[]={bb_reflection__intClass,bb_reflection__classes[0]};
	c_MethodInfo::m_new(String(L"Add",3),0,bb_reflection__boolClass,Array<c_ClassInfo* >(t_,2));
	return this;
}
void c_R362::mark(){
	c_MethodInfo::mark();
}
c_R363::c_R363(){
}
c_R363* c_R363::m_new(){
	c_ClassInfo* t_[]={bb_reflection__intClass,bb_reflection__classes[0]};
	c_MethodInfo::m_new(String(L"Update",6),0,bb_reflection__boolClass,Array<c_ClassInfo* >(t_,2));
	return this;
}
void c_R363::mark(){
	c_MethodInfo::mark();
}
c_R364::c_R364(){
}
c_R364* c_R364::m_new(){
	c_ClassInfo* t_[]={bb_reflection__intClass};
	c_MethodInfo::m_new(String(L"Get",3),0,bb_reflection__classes[0],Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R364::mark(){
	c_MethodInfo::mark();
}
c_R365::c_R365(){
}
c_R365* c_R365::m_new(){
	c_ClassInfo* t_[]={bb_reflection__intClass};
	c_MethodInfo::m_new(String(L"Remove",6),0,bb_reflection__intClass,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R365::mark(){
	c_MethodInfo::mark();
}
c_R366::c_R366(){
}
c_R366* c_R366::m_new(){
	c_MethodInfo::m_new(String(L"Keys",4),0,bb_reflection__classes[69],Array<c_ClassInfo* >());
	return this;
}
void c_R366::mark(){
	c_MethodInfo::mark();
}
c_R367::c_R367(){
}
c_R367* c_R367::m_new(){
	c_MethodInfo::m_new(String(L"Values",6),0,bb_reflection__classes[70],Array<c_ClassInfo* >());
	return this;
}
void c_R367::mark(){
	c_MethodInfo::mark();
}
c_R368::c_R368(){
}
c_R368* c_R368::m_new(){
	c_MethodInfo::m_new(String(L"ObjectEnumerator",16),0,bb_reflection__classes[71],Array<c_ClassInfo* >());
	return this;
}
void c_R368::mark(){
	c_MethodInfo::mark();
}
c_R369::c_R369(){
}
c_R369* c_R369::m_new(){
	c_ClassInfo* t_[]={bb_reflection__intClass,bb_reflection__classes[0]};
	c_MethodInfo::m_new(String(L"Insert",6),0,bb_reflection__boolClass,Array<c_ClassInfo* >(t_,2));
	return this;
}
void c_R369::mark(){
	c_MethodInfo::mark();
}
c_R370::c_R370(){
}
c_R370* c_R370::m_new(){
	c_ClassInfo* t_[]={bb_reflection__intClass};
	c_MethodInfo::m_new(String(L"ValueForKey",11),0,bb_reflection__classes[0],Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R370::mark(){
	c_MethodInfo::mark();
}
c_R371::c_R371(){
}
c_R371* c_R371::m_new(){
	c_MethodInfo::m_new(String(L"FirstNode",9),0,bb_reflection__classes[68],Array<c_ClassInfo* >());
	return this;
}
void c_R371::mark(){
	c_MethodInfo::mark();
}
c_R372::c_R372(){
}
c_R372* c_R372::m_new(){
	c_MethodInfo::m_new(String(L"LastNode",8),0,bb_reflection__classes[68],Array<c_ClassInfo* >());
	return this;
}
void c_R372::mark(){
	c_MethodInfo::mark();
}
c_R373::c_R373(){
}
c_R373* c_R373::m_new(){
	c_ClassInfo* t_[]={bb_reflection__intClass};
	c_MethodInfo::m_new(String(L"FindNode",8),0,bb_reflection__classes[68],Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R373::mark(){
	c_MethodInfo::mark();
}
c_R374::c_R374(){
}
c_R374* c_R374::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[68]};
	c_MethodInfo::m_new(String(L"RemoveNode",10),0,bb_reflection__intClass,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R374::mark(){
	c_MethodInfo::mark();
}
c_R375::c_R375(){
}
c_R375* c_R375::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[68]};
	c_MethodInfo::m_new(String(L"InsertFixup",11),0,bb_reflection__intClass,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R375::mark(){
	c_MethodInfo::mark();
}
c_R376::c_R376(){
}
c_R376* c_R376::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[68]};
	c_MethodInfo::m_new(String(L"RotateLeft",10),0,bb_reflection__intClass,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R376::mark(){
	c_MethodInfo::mark();
}
c_R377::c_R377(){
}
c_R377* c_R377::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[68]};
	c_MethodInfo::m_new(String(L"RotateRight",11),0,bb_reflection__intClass,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R377::mark(){
	c_MethodInfo::mark();
}
c_R378::c_R378(){
}
c_R378* c_R378::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[68],bb_reflection__classes[68]};
	c_MethodInfo::m_new(String(L"DeleteFixup",11),0,bb_reflection__intClass,Array<c_ClassInfo* >(t_,2));
	return this;
}
void c_R378::mark(){
	c_MethodInfo::mark();
}
c_R380::c_R380(){
}
c_R380* c_R380::m_new(){
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[27],Array<c_ClassInfo* >());
	return this;
}
void c_R380::mark(){
	c_FunctionInfo::mark();
}
c_R382::c_R382(){
}
c_R382* c_R382::m_new(){
	c_ClassInfo* t_[]={bb_reflection__intClass,bb_reflection__intClass};
	c_MethodInfo::m_new(String(L"Compare",7),0,bb_reflection__intClass,Array<c_ClassInfo* >(t_,2));
	return this;
}
void c_R382::mark(){
	c_MethodInfo::mark();
}
c_R383::c_R383(){
}
c_R383* c_R383::m_new(){
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[28],Array<c_ClassInfo* >());
	return this;
}
void c_R383::mark(){
	c_FunctionInfo::mark();
}
c_R393::c_R393(){
}
c_R393* c_R393::m_new(){
	c_FieldInfo::m_new(String(L"map",3),2,bb_reflection__classes[31]);
	return this;
}
void c_R393::mark(){
	c_FieldInfo::mark();
}
c_R386::c_R386(){
}
c_R386* c_R386::m_new(){
	c_MethodInfo::m_new(String(L"Clear",5),0,bb_reflection__intClass,Array<c_ClassInfo* >());
	return this;
}
void c_R386::mark(){
	c_MethodInfo::mark();
}
c_R387::c_R387(){
}
c_R387* c_R387::m_new(){
	c_MethodInfo::m_new(String(L"Count",5),0,bb_reflection__intClass,Array<c_ClassInfo* >());
	return this;
}
void c_R387::mark(){
	c_MethodInfo::mark();
}
c_R388::c_R388(){
}
c_R388* c_R388::m_new(){
	c_MethodInfo::m_new(String(L"IsEmpty",7),0,bb_reflection__boolClass,Array<c_ClassInfo* >());
	return this;
}
void c_R388::mark(){
	c_MethodInfo::mark();
}
c_R389::c_R389(){
}
c_R389* c_R389::m_new(){
	c_ClassInfo* t_[]={bb_reflection__floatClass};
	c_MethodInfo::m_new(String(L"Contains",8),0,bb_reflection__boolClass,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R389::mark(){
	c_MethodInfo::mark();
}
c_R390::c_R390(){
}
c_R390* c_R390::m_new(){
	c_ClassInfo* t_[]={bb_reflection__floatClass};
	c_MethodInfo::m_new(String(L"Insert",6),0,bb_reflection__intClass,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R390::mark(){
	c_MethodInfo::mark();
}
c_R391::c_R391(){
}
c_R391* c_R391::m_new(){
	c_ClassInfo* t_[]={bb_reflection__floatClass};
	c_MethodInfo::m_new(String(L"Remove",6),0,bb_reflection__intClass,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R391::mark(){
	c_MethodInfo::mark();
}
c_R392::c_R392(){
}
c_R392* c_R392::m_new(){
	c_MethodInfo::m_new(String(L"ObjectEnumerator",16),0,bb_reflection__classes[83],Array<c_ClassInfo* >());
	return this;
}
void c_R392::mark(){
	c_MethodInfo::mark();
}
c_R385::c_R385(){
}
c_R385* c_R385::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[31]};
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[29],Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R385::mark(){
	c_FunctionInfo::mark();
}
c_R394::c_R394(){
}
c_R394* c_R394::m_new(){
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[29],Array<c_ClassInfo* >());
	return this;
}
void c_R394::mark(){
	c_FunctionInfo::mark();
}
c_R396::c_R396(){
}
c_R396* c_R396::m_new(){
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[30],Array<c_ClassInfo* >());
	return this;
}
void c_R396::mark(){
	c_FunctionInfo::mark();
}
c_R421::c_R421(){
}
c_R421* c_R421::m_new(){
	c_FieldInfo::m_new(String(L"root",4),2,bb_reflection__classes[72]);
	return this;
}
void c_R421::mark(){
	c_FieldInfo::mark();
}
c_R398::c_R398(){
}
c_R398* c_R398::m_new(){
	c_ClassInfo* t_[]={bb_reflection__floatClass,bb_reflection__floatClass};
	c_MethodInfo::m_new(String(L"Compare",7),4,bb_reflection__intClass,Array<c_ClassInfo* >(t_,2));
	return this;
}
void c_R398::mark(){
	c_MethodInfo::mark();
}
c_R399::c_R399(){
}
c_R399* c_R399::m_new(){
	c_MethodInfo::m_new(String(L"Clear",5),0,bb_reflection__intClass,Array<c_ClassInfo* >());
	return this;
}
void c_R399::mark(){
	c_MethodInfo::mark();
}
c_R400::c_R400(){
}
c_R400* c_R400::m_new(){
	c_MethodInfo::m_new(String(L"Count",5),0,bb_reflection__intClass,Array<c_ClassInfo* >());
	return this;
}
void c_R400::mark(){
	c_MethodInfo::mark();
}
c_R401::c_R401(){
}
c_R401* c_R401::m_new(){
	c_MethodInfo::m_new(String(L"IsEmpty",7),0,bb_reflection__boolClass,Array<c_ClassInfo* >());
	return this;
}
void c_R401::mark(){
	c_MethodInfo::mark();
}
c_R402::c_R402(){
}
c_R402* c_R402::m_new(){
	c_ClassInfo* t_[]={bb_reflection__floatClass};
	c_MethodInfo::m_new(String(L"Contains",8),0,bb_reflection__boolClass,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R402::mark(){
	c_MethodInfo::mark();
}
c_R403::c_R403(){
}
c_R403* c_R403::m_new(){
	c_ClassInfo* t_[]={bb_reflection__floatClass,bb_reflection__classes[0]};
	c_MethodInfo::m_new(String(L"Set",3),0,bb_reflection__boolClass,Array<c_ClassInfo* >(t_,2));
	return this;
}
void c_R403::mark(){
	c_MethodInfo::mark();
}
c_R404::c_R404(){
}
c_R404* c_R404::m_new(){
	c_ClassInfo* t_[]={bb_reflection__floatClass,bb_reflection__classes[0]};
	c_MethodInfo::m_new(String(L"Add",3),0,bb_reflection__boolClass,Array<c_ClassInfo* >(t_,2));
	return this;
}
void c_R404::mark(){
	c_MethodInfo::mark();
}
c_R405::c_R405(){
}
c_R405* c_R405::m_new(){
	c_ClassInfo* t_[]={bb_reflection__floatClass,bb_reflection__classes[0]};
	c_MethodInfo::m_new(String(L"Update",6),0,bb_reflection__boolClass,Array<c_ClassInfo* >(t_,2));
	return this;
}
void c_R405::mark(){
	c_MethodInfo::mark();
}
c_R406::c_R406(){
}
c_R406* c_R406::m_new(){
	c_ClassInfo* t_[]={bb_reflection__floatClass};
	c_MethodInfo::m_new(String(L"Get",3),0,bb_reflection__classes[0],Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R406::mark(){
	c_MethodInfo::mark();
}
c_R407::c_R407(){
}
c_R407* c_R407::m_new(){
	c_ClassInfo* t_[]={bb_reflection__floatClass};
	c_MethodInfo::m_new(String(L"Remove",6),0,bb_reflection__intClass,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R407::mark(){
	c_MethodInfo::mark();
}
c_R408::c_R408(){
}
c_R408* c_R408::m_new(){
	c_MethodInfo::m_new(String(L"Keys",4),0,bb_reflection__classes[73],Array<c_ClassInfo* >());
	return this;
}
void c_R408::mark(){
	c_MethodInfo::mark();
}
c_R409::c_R409(){
}
c_R409* c_R409::m_new(){
	c_MethodInfo::m_new(String(L"Values",6),0,bb_reflection__classes[74],Array<c_ClassInfo* >());
	return this;
}
void c_R409::mark(){
	c_MethodInfo::mark();
}
c_R410::c_R410(){
}
c_R410* c_R410::m_new(){
	c_MethodInfo::m_new(String(L"ObjectEnumerator",16),0,bb_reflection__classes[75],Array<c_ClassInfo* >());
	return this;
}
void c_R410::mark(){
	c_MethodInfo::mark();
}
c_R411::c_R411(){
}
c_R411* c_R411::m_new(){
	c_ClassInfo* t_[]={bb_reflection__floatClass,bb_reflection__classes[0]};
	c_MethodInfo::m_new(String(L"Insert",6),0,bb_reflection__boolClass,Array<c_ClassInfo* >(t_,2));
	return this;
}
void c_R411::mark(){
	c_MethodInfo::mark();
}
c_R412::c_R412(){
}
c_R412* c_R412::m_new(){
	c_ClassInfo* t_[]={bb_reflection__floatClass};
	c_MethodInfo::m_new(String(L"ValueForKey",11),0,bb_reflection__classes[0],Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R412::mark(){
	c_MethodInfo::mark();
}
c_R413::c_R413(){
}
c_R413* c_R413::m_new(){
	c_MethodInfo::m_new(String(L"FirstNode",9),0,bb_reflection__classes[72],Array<c_ClassInfo* >());
	return this;
}
void c_R413::mark(){
	c_MethodInfo::mark();
}
c_R414::c_R414(){
}
c_R414* c_R414::m_new(){
	c_MethodInfo::m_new(String(L"LastNode",8),0,bb_reflection__classes[72],Array<c_ClassInfo* >());
	return this;
}
void c_R414::mark(){
	c_MethodInfo::mark();
}
c_R415::c_R415(){
}
c_R415* c_R415::m_new(){
	c_ClassInfo* t_[]={bb_reflection__floatClass};
	c_MethodInfo::m_new(String(L"FindNode",8),0,bb_reflection__classes[72],Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R415::mark(){
	c_MethodInfo::mark();
}
c_R416::c_R416(){
}
c_R416* c_R416::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[72]};
	c_MethodInfo::m_new(String(L"RemoveNode",10),0,bb_reflection__intClass,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R416::mark(){
	c_MethodInfo::mark();
}
c_R417::c_R417(){
}
c_R417* c_R417::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[72]};
	c_MethodInfo::m_new(String(L"InsertFixup",11),0,bb_reflection__intClass,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R417::mark(){
	c_MethodInfo::mark();
}
c_R418::c_R418(){
}
c_R418* c_R418::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[72]};
	c_MethodInfo::m_new(String(L"RotateLeft",10),0,bb_reflection__intClass,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R418::mark(){
	c_MethodInfo::mark();
}
c_R419::c_R419(){
}
c_R419* c_R419::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[72]};
	c_MethodInfo::m_new(String(L"RotateRight",11),0,bb_reflection__intClass,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R419::mark(){
	c_MethodInfo::mark();
}
c_R420::c_R420(){
}
c_R420* c_R420::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[72],bb_reflection__classes[72]};
	c_MethodInfo::m_new(String(L"DeleteFixup",11),0,bb_reflection__intClass,Array<c_ClassInfo* >(t_,2));
	return this;
}
void c_R420::mark(){
	c_MethodInfo::mark();
}
c_R422::c_R422(){
}
c_R422* c_R422::m_new(){
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[31],Array<c_ClassInfo* >());
	return this;
}
void c_R422::mark(){
	c_FunctionInfo::mark();
}
c_R424::c_R424(){
}
c_R424* c_R424::m_new(){
	c_ClassInfo* t_[]={bb_reflection__floatClass,bb_reflection__floatClass};
	c_MethodInfo::m_new(String(L"Compare",7),0,bb_reflection__intClass,Array<c_ClassInfo* >(t_,2));
	return this;
}
void c_R424::mark(){
	c_MethodInfo::mark();
}
c_R425::c_R425(){
}
c_R425* c_R425::m_new(){
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[32],Array<c_ClassInfo* >());
	return this;
}
void c_R425::mark(){
	c_FunctionInfo::mark();
}
c_R435::c_R435(){
}
c_R435* c_R435::m_new(){
	c_FieldInfo::m_new(String(L"map",3),2,bb_reflection__classes[35]);
	return this;
}
void c_R435::mark(){
	c_FieldInfo::mark();
}
c_R428::c_R428(){
}
c_R428* c_R428::m_new(){
	c_MethodInfo::m_new(String(L"Clear",5),0,bb_reflection__intClass,Array<c_ClassInfo* >());
	return this;
}
void c_R428::mark(){
	c_MethodInfo::mark();
}
c_R429::c_R429(){
}
c_R429* c_R429::m_new(){
	c_MethodInfo::m_new(String(L"Count",5),0,bb_reflection__intClass,Array<c_ClassInfo* >());
	return this;
}
void c_R429::mark(){
	c_MethodInfo::mark();
}
c_R430::c_R430(){
}
c_R430* c_R430::m_new(){
	c_MethodInfo::m_new(String(L"IsEmpty",7),0,bb_reflection__boolClass,Array<c_ClassInfo* >());
	return this;
}
void c_R430::mark(){
	c_MethodInfo::mark();
}
c_R431::c_R431(){
}
c_R431* c_R431::m_new(){
	c_ClassInfo* t_[]={bb_reflection__stringClass};
	c_MethodInfo::m_new(String(L"Contains",8),0,bb_reflection__boolClass,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R431::mark(){
	c_MethodInfo::mark();
}
c_R432::c_R432(){
}
c_R432* c_R432::m_new(){
	c_ClassInfo* t_[]={bb_reflection__stringClass};
	c_MethodInfo::m_new(String(L"Insert",6),0,bb_reflection__intClass,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R432::mark(){
	c_MethodInfo::mark();
}
c_R433::c_R433(){
}
c_R433* c_R433::m_new(){
	c_ClassInfo* t_[]={bb_reflection__stringClass};
	c_MethodInfo::m_new(String(L"Remove",6),0,bb_reflection__intClass,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R433::mark(){
	c_MethodInfo::mark();
}
c_R434::c_R434(){
}
c_R434* c_R434::m_new(){
	c_MethodInfo::m_new(String(L"ObjectEnumerator",16),0,bb_reflection__classes[84],Array<c_ClassInfo* >());
	return this;
}
void c_R434::mark(){
	c_MethodInfo::mark();
}
c_R427::c_R427(){
}
c_R427* c_R427::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[35]};
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[33],Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R427::mark(){
	c_FunctionInfo::mark();
}
c_R436::c_R436(){
}
c_R436* c_R436::m_new(){
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[33],Array<c_ClassInfo* >());
	return this;
}
void c_R436::mark(){
	c_FunctionInfo::mark();
}
c_R438::c_R438(){
}
c_R438* c_R438::m_new(){
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[34],Array<c_ClassInfo* >());
	return this;
}
void c_R438::mark(){
	c_FunctionInfo::mark();
}
c_R463::c_R463(){
}
c_R463* c_R463::m_new(){
	c_FieldInfo::m_new(String(L"root",4),2,bb_reflection__classes[76]);
	return this;
}
void c_R463::mark(){
	c_FieldInfo::mark();
}
c_R440::c_R440(){
}
c_R440* c_R440::m_new(){
	c_ClassInfo* t_[]={bb_reflection__stringClass,bb_reflection__stringClass};
	c_MethodInfo::m_new(String(L"Compare",7),4,bb_reflection__intClass,Array<c_ClassInfo* >(t_,2));
	return this;
}
void c_R440::mark(){
	c_MethodInfo::mark();
}
c_R441::c_R441(){
}
c_R441* c_R441::m_new(){
	c_MethodInfo::m_new(String(L"Clear",5),0,bb_reflection__intClass,Array<c_ClassInfo* >());
	return this;
}
void c_R441::mark(){
	c_MethodInfo::mark();
}
c_R442::c_R442(){
}
c_R442* c_R442::m_new(){
	c_MethodInfo::m_new(String(L"Count",5),0,bb_reflection__intClass,Array<c_ClassInfo* >());
	return this;
}
void c_R442::mark(){
	c_MethodInfo::mark();
}
c_R443::c_R443(){
}
c_R443* c_R443::m_new(){
	c_MethodInfo::m_new(String(L"IsEmpty",7),0,bb_reflection__boolClass,Array<c_ClassInfo* >());
	return this;
}
void c_R443::mark(){
	c_MethodInfo::mark();
}
c_R444::c_R444(){
}
c_R444* c_R444::m_new(){
	c_ClassInfo* t_[]={bb_reflection__stringClass};
	c_MethodInfo::m_new(String(L"Contains",8),0,bb_reflection__boolClass,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R444::mark(){
	c_MethodInfo::mark();
}
c_R445::c_R445(){
}
c_R445* c_R445::m_new(){
	c_ClassInfo* t_[]={bb_reflection__stringClass,bb_reflection__classes[0]};
	c_MethodInfo::m_new(String(L"Set",3),0,bb_reflection__boolClass,Array<c_ClassInfo* >(t_,2));
	return this;
}
void c_R445::mark(){
	c_MethodInfo::mark();
}
c_R446::c_R446(){
}
c_R446* c_R446::m_new(){
	c_ClassInfo* t_[]={bb_reflection__stringClass,bb_reflection__classes[0]};
	c_MethodInfo::m_new(String(L"Add",3),0,bb_reflection__boolClass,Array<c_ClassInfo* >(t_,2));
	return this;
}
void c_R446::mark(){
	c_MethodInfo::mark();
}
c_R447::c_R447(){
}
c_R447* c_R447::m_new(){
	c_ClassInfo* t_[]={bb_reflection__stringClass,bb_reflection__classes[0]};
	c_MethodInfo::m_new(String(L"Update",6),0,bb_reflection__boolClass,Array<c_ClassInfo* >(t_,2));
	return this;
}
void c_R447::mark(){
	c_MethodInfo::mark();
}
c_R448::c_R448(){
}
c_R448* c_R448::m_new(){
	c_ClassInfo* t_[]={bb_reflection__stringClass};
	c_MethodInfo::m_new(String(L"Get",3),0,bb_reflection__classes[0],Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R448::mark(){
	c_MethodInfo::mark();
}
c_R449::c_R449(){
}
c_R449* c_R449::m_new(){
	c_ClassInfo* t_[]={bb_reflection__stringClass};
	c_MethodInfo::m_new(String(L"Remove",6),0,bb_reflection__intClass,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R449::mark(){
	c_MethodInfo::mark();
}
c_R450::c_R450(){
}
c_R450* c_R450::m_new(){
	c_MethodInfo::m_new(String(L"Keys",4),0,bb_reflection__classes[77],Array<c_ClassInfo* >());
	return this;
}
void c_R450::mark(){
	c_MethodInfo::mark();
}
c_R451::c_R451(){
}
c_R451* c_R451::m_new(){
	c_MethodInfo::m_new(String(L"Values",6),0,bb_reflection__classes[78],Array<c_ClassInfo* >());
	return this;
}
void c_R451::mark(){
	c_MethodInfo::mark();
}
c_R452::c_R452(){
}
c_R452* c_R452::m_new(){
	c_MethodInfo::m_new(String(L"ObjectEnumerator",16),0,bb_reflection__classes[79],Array<c_ClassInfo* >());
	return this;
}
void c_R452::mark(){
	c_MethodInfo::mark();
}
c_R453::c_R453(){
}
c_R453* c_R453::m_new(){
	c_ClassInfo* t_[]={bb_reflection__stringClass,bb_reflection__classes[0]};
	c_MethodInfo::m_new(String(L"Insert",6),0,bb_reflection__boolClass,Array<c_ClassInfo* >(t_,2));
	return this;
}
void c_R453::mark(){
	c_MethodInfo::mark();
}
c_R454::c_R454(){
}
c_R454* c_R454::m_new(){
	c_ClassInfo* t_[]={bb_reflection__stringClass};
	c_MethodInfo::m_new(String(L"ValueForKey",11),0,bb_reflection__classes[0],Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R454::mark(){
	c_MethodInfo::mark();
}
c_R455::c_R455(){
}
c_R455* c_R455::m_new(){
	c_MethodInfo::m_new(String(L"FirstNode",9),0,bb_reflection__classes[76],Array<c_ClassInfo* >());
	return this;
}
void c_R455::mark(){
	c_MethodInfo::mark();
}
c_R456::c_R456(){
}
c_R456* c_R456::m_new(){
	c_MethodInfo::m_new(String(L"LastNode",8),0,bb_reflection__classes[76],Array<c_ClassInfo* >());
	return this;
}
void c_R456::mark(){
	c_MethodInfo::mark();
}
c_R457::c_R457(){
}
c_R457* c_R457::m_new(){
	c_ClassInfo* t_[]={bb_reflection__stringClass};
	c_MethodInfo::m_new(String(L"FindNode",8),0,bb_reflection__classes[76],Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R457::mark(){
	c_MethodInfo::mark();
}
c_R458::c_R458(){
}
c_R458* c_R458::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[76]};
	c_MethodInfo::m_new(String(L"RemoveNode",10),0,bb_reflection__intClass,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R458::mark(){
	c_MethodInfo::mark();
}
c_R459::c_R459(){
}
c_R459* c_R459::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[76]};
	c_MethodInfo::m_new(String(L"InsertFixup",11),0,bb_reflection__intClass,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R459::mark(){
	c_MethodInfo::mark();
}
c_R460::c_R460(){
}
c_R460* c_R460::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[76]};
	c_MethodInfo::m_new(String(L"RotateLeft",10),0,bb_reflection__intClass,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R460::mark(){
	c_MethodInfo::mark();
}
c_R461::c_R461(){
}
c_R461* c_R461::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[76]};
	c_MethodInfo::m_new(String(L"RotateRight",11),0,bb_reflection__intClass,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R461::mark(){
	c_MethodInfo::mark();
}
c_R462::c_R462(){
}
c_R462* c_R462::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[76],bb_reflection__classes[76]};
	c_MethodInfo::m_new(String(L"DeleteFixup",11),0,bb_reflection__intClass,Array<c_ClassInfo* >(t_,2));
	return this;
}
void c_R462::mark(){
	c_MethodInfo::mark();
}
c_R464::c_R464(){
}
c_R464* c_R464::m_new(){
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[35],Array<c_ClassInfo* >());
	return this;
}
void c_R464::mark(){
	c_FunctionInfo::mark();
}
c_R466::c_R466(){
}
c_R466* c_R466::m_new(){
	c_ClassInfo* t_[]={bb_reflection__stringClass,bb_reflection__stringClass};
	c_MethodInfo::m_new(String(L"Compare",7),0,bb_reflection__intClass,Array<c_ClassInfo* >(t_,2));
	return this;
}
void c_R466::mark(){
	c_MethodInfo::mark();
}
c_R467::c_R467(){
}
c_R467* c_R467::m_new(){
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[36],Array<c_ClassInfo* >());
	return this;
}
void c_R467::mark(){
	c_FunctionInfo::mark();
}
c_R497::c_R497(){
}
c_R497* c_R497::m_new(){
	c_GlobalInfo::m_new(String(L"NIL",3),2,bb_reflection__intClass);
	return this;
}
void c_R497::mark(){
	c_GlobalInfo::mark();
}
c_R498::c_R498(){
}
c_R498* c_R498::m_new(){
	c_FieldInfo::m_new(String(L"data",4),2,bb_reflection__classes[98]);
	return this;
}
void c_R498::mark(){
	c_FieldInfo::mark();
}
c_R499::c_R499(){
}
c_R499* c_R499::m_new(){
	c_FieldInfo::m_new(String(L"length",6),2,bb_reflection__intClass);
	return this;
}
void c_R499::mark(){
	c_FieldInfo::mark();
}
c_R471::c_R471(){
}
c_R471* c_R471::m_new(){
	c_MethodInfo::m_new(String(L"ToArray",7),0,bb_reflection__classes[98],Array<c_ClassInfo* >());
	return this;
}
void c_R471::mark(){
	c_MethodInfo::mark();
}
c_R472::c_R472(){
}
c_R472* c_R472::m_new(){
	c_ClassInfo* t_[]={bb_reflection__intClass,bb_reflection__intClass};
	c_MethodInfo::m_new(String(L"Equals",6),0,bb_reflection__boolClass,Array<c_ClassInfo* >(t_,2));
	return this;
}
void c_R472::mark(){
	c_MethodInfo::mark();
}
c_R473::c_R473(){
}
c_R473* c_R473::m_new(){
	c_ClassInfo* t_[]={bb_reflection__intClass,bb_reflection__intClass};
	c_MethodInfo::m_new(String(L"Compare",7),0,bb_reflection__intClass,Array<c_ClassInfo* >(t_,2));
	return this;
}
void c_R473::mark(){
	c_MethodInfo::mark();
}
c_R474::c_R474(){
}
c_R474* c_R474::m_new(){
	c_MethodInfo::m_new(String(L"Clear",5),0,0,Array<c_ClassInfo* >());
	return this;
}
void c_R474::mark(){
	c_MethodInfo::mark();
}
c_R475::c_R475(){
}
c_R475* c_R475::m_new(){
	c_ClassInfo* t_[]={bb_reflection__intClass};
	c_MethodInfo::m_new(String(L"Length",6),0,0,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R475::mark(){
	c_MethodInfo::mark();
}
c_R476::c_R476(){
}
c_R476* c_R476::m_new(){
	c_MethodInfo::m_new(String(L"Length",6),0,bb_reflection__intClass,Array<c_ClassInfo* >());
	return this;
}
void c_R476::mark(){
	c_MethodInfo::mark();
}
c_R477::c_R477(){
}
c_R477* c_R477::m_new(){
	c_MethodInfo::m_new(String(L"IsEmpty",7),0,bb_reflection__boolClass,Array<c_ClassInfo* >());
	return this;
}
void c_R477::mark(){
	c_MethodInfo::mark();
}
c_R478::c_R478(){
}
c_R478* c_R478::m_new(){
	c_ClassInfo* t_[]={bb_reflection__intClass};
	c_MethodInfo::m_new(String(L"Contains",8),0,bb_reflection__boolClass,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R478::mark(){
	c_MethodInfo::mark();
}
c_R479::c_R479(){
}
c_R479* c_R479::m_new(){
	c_ClassInfo* t_[]={bb_reflection__intClass};
	c_MethodInfo::m_new(String(L"Push",4),0,0,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R479::mark(){
	c_MethodInfo::mark();
}
c_R480::c_R480(){
}
c_R480* c_R480::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[98],bb_reflection__intClass};
	c_MethodInfo::m_new(String(L"Push",4),0,0,Array<c_ClassInfo* >(t_,2));
	return this;
}
void c_R480::mark(){
	c_MethodInfo::mark();
}
c_R481::c_R481(){
}
c_R481* c_R481::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[98],bb_reflection__intClass,bb_reflection__intClass};
	c_MethodInfo::m_new(String(L"Push",4),0,0,Array<c_ClassInfo* >(t_,3));
	return this;
}
void c_R481::mark(){
	c_MethodInfo::mark();
}
c_R482::c_R482(){
}
c_R482* c_R482::m_new(){
	c_MethodInfo::m_new(String(L"Pop",3),0,bb_reflection__intClass,Array<c_ClassInfo* >());
	return this;
}
void c_R482::mark(){
	c_MethodInfo::mark();
}
c_R483::c_R483(){
}
c_R483* c_R483::m_new(){
	c_MethodInfo::m_new(String(L"Top",3),0,bb_reflection__intClass,Array<c_ClassInfo* >());
	return this;
}
void c_R483::mark(){
	c_MethodInfo::mark();
}
c_R484::c_R484(){
}
c_R484* c_R484::m_new(){
	c_ClassInfo* t_[]={bb_reflection__intClass,bb_reflection__intClass};
	c_MethodInfo::m_new(String(L"Set",3),0,0,Array<c_ClassInfo* >(t_,2));
	return this;
}
void c_R484::mark(){
	c_MethodInfo::mark();
}
c_R485::c_R485(){
}
c_R485* c_R485::m_new(){
	c_ClassInfo* t_[]={bb_reflection__intClass};
	c_MethodInfo::m_new(String(L"Get",3),0,bb_reflection__intClass,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R485::mark(){
	c_MethodInfo::mark();
}
c_R486::c_R486(){
}
c_R486* c_R486::m_new(){
	c_ClassInfo* t_[]={bb_reflection__intClass,bb_reflection__intClass};
	c_MethodInfo::m_new(String(L"Find",4),0,bb_reflection__intClass,Array<c_ClassInfo* >(t_,2));
	return this;
}
void c_R486::mark(){
	c_MethodInfo::mark();
}
c_R487::c_R487(){
}
c_R487* c_R487::m_new(){
	c_ClassInfo* t_[]={bb_reflection__intClass};
	c_MethodInfo::m_new(String(L"FindLast",8),0,bb_reflection__intClass,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R487::mark(){
	c_MethodInfo::mark();
}
c_R488::c_R488(){
}
c_R488* c_R488::m_new(){
	c_ClassInfo* t_[]={bb_reflection__intClass,bb_reflection__intClass};
	c_MethodInfo::m_new(String(L"FindLast",8),0,bb_reflection__intClass,Array<c_ClassInfo* >(t_,2));
	return this;
}
void c_R488::mark(){
	c_MethodInfo::mark();
}
c_R489::c_R489(){
}
c_R489* c_R489::m_new(){
	c_ClassInfo* t_[]={bb_reflection__intClass,bb_reflection__intClass};
	c_MethodInfo::m_new(String(L"Insert",6),0,0,Array<c_ClassInfo* >(t_,2));
	return this;
}
void c_R489::mark(){
	c_MethodInfo::mark();
}
c_R490::c_R490(){
}
c_R490* c_R490::m_new(){
	c_ClassInfo* t_[]={bb_reflection__intClass};
	c_MethodInfo::m_new(String(L"Remove",6),0,0,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R490::mark(){
	c_MethodInfo::mark();
}
c_R491::c_R491(){
}
c_R491* c_R491::m_new(){
	c_ClassInfo* t_[]={bb_reflection__intClass};
	c_MethodInfo::m_new(String(L"RemoveFirst",11),0,0,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R491::mark(){
	c_MethodInfo::mark();
}
c_R492::c_R492(){
}
c_R492* c_R492::m_new(){
	c_ClassInfo* t_[]={bb_reflection__intClass};
	c_MethodInfo::m_new(String(L"RemoveLast",10),0,0,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R492::mark(){
	c_MethodInfo::mark();
}
c_R493::c_R493(){
}
c_R493* c_R493::m_new(){
	c_ClassInfo* t_[]={bb_reflection__intClass};
	c_MethodInfo::m_new(String(L"RemoveEach",10),0,0,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R493::mark(){
	c_MethodInfo::mark();
}
c_R494::c_R494(){
}
c_R494* c_R494::m_new(){
	c_ClassInfo* t_[]={bb_reflection__boolClass};
	c_MethodInfo::m_new(String(L"Sort",4),0,0,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R494::mark(){
	c_MethodInfo::mark();
}
c_R495::c_R495(){
}
c_R495* c_R495::m_new(){
	c_MethodInfo::m_new(String(L"ObjectEnumerator",16),0,bb_reflection__classes[89],Array<c_ClassInfo* >());
	return this;
}
void c_R495::mark(){
	c_MethodInfo::mark();
}
c_R496::c_R496(){
}
c_R496* c_R496::m_new(){
	c_MethodInfo::m_new(String(L"Backwards",9),0,bb_reflection__classes[90],Array<c_ClassInfo* >());
	return this;
}
void c_R496::mark(){
	c_MethodInfo::mark();
}
c_R500::c_R500(){
}
c_R500* c_R500::m_new(){
	c_ClassInfo* t_[]={bb_reflection__intClass,bb_reflection__intClass};
	c_MethodInfo::m_new(String(L"_Swap",5),8,0,Array<c_ClassInfo* >(t_,2));
	return this;
}
void c_R500::mark(){
	c_MethodInfo::mark();
}
c_R501::c_R501(){
}
c_R501* c_R501::m_new(){
	c_ClassInfo* t_[]={bb_reflection__intClass,bb_reflection__intClass,bb_reflection__intClass};
	c_MethodInfo::m_new(String(L"_Less",5),8,bb_reflection__boolClass,Array<c_ClassInfo* >(t_,3));
	return this;
}
void c_R501::mark(){
	c_MethodInfo::mark();
}
c_R502::c_R502(){
}
c_R502* c_R502::m_new(){
	c_ClassInfo* t_[]={bb_reflection__intClass,bb_reflection__intClass,bb_reflection__intClass};
	c_MethodInfo::m_new(String(L"_Less2",6),8,bb_reflection__boolClass,Array<c_ClassInfo* >(t_,3));
	return this;
}
void c_R502::mark(){
	c_MethodInfo::mark();
}
c_R503::c_R503(){
}
c_R503* c_R503::m_new(){
	c_ClassInfo* t_[]={bb_reflection__intClass,bb_reflection__intClass,bb_reflection__intClass};
	c_MethodInfo::m_new(String(L"_Less3",6),8,bb_reflection__boolClass,Array<c_ClassInfo* >(t_,3));
	return this;
}
void c_R503::mark(){
	c_MethodInfo::mark();
}
c_R504::c_R504(){
}
c_R504* c_R504::m_new(){
	c_ClassInfo* t_[]={bb_reflection__intClass,bb_reflection__intClass,bb_reflection__intClass};
	c_MethodInfo::m_new(String(L"_Sort",5),8,0,Array<c_ClassInfo* >(t_,3));
	return this;
}
void c_R504::mark(){
	c_MethodInfo::mark();
}
c_R469::c_R469(){
}
c_R469* c_R469::m_new(){
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[37],Array<c_ClassInfo* >());
	return this;
}
void c_R469::mark(){
	c_FunctionInfo::mark();
}
c_R470::c_R470(){
}
c_R470* c_R470::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[98]};
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[37],Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R470::mark(){
	c_FunctionInfo::mark();
}
c_R507::c_R507(){
}
c_R507* c_R507::m_new(){
	c_ClassInfo* t_[]={bb_reflection__intClass,bb_reflection__intClass};
	c_MethodInfo::m_new(String(L"Equals",6),0,bb_reflection__boolClass,Array<c_ClassInfo* >(t_,2));
	return this;
}
void c_R507::mark(){
	c_MethodInfo::mark();
}
c_R508::c_R508(){
}
c_R508* c_R508::m_new(){
	c_ClassInfo* t_[]={bb_reflection__intClass,bb_reflection__intClass};
	c_MethodInfo::m_new(String(L"Compare",7),0,bb_reflection__intClass,Array<c_ClassInfo* >(t_,2));
	return this;
}
void c_R508::mark(){
	c_MethodInfo::mark();
}
c_R506::c_R506(){
}
c_R506* c_R506::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[98]};
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[38],Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R506::mark(){
	c_FunctionInfo::mark();
}
c_R509::c_R509(){
}
c_R509* c_R509::m_new(){
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[38],Array<c_ClassInfo* >());
	return this;
}
void c_R509::mark(){
	c_FunctionInfo::mark();
}
c_R539::c_R539(){
}
c_R539* c_R539::m_new(){
	c_GlobalInfo::m_new(String(L"NIL",3),2,bb_reflection__floatClass);
	return this;
}
void c_R539::mark(){
	c_GlobalInfo::mark();
}
c_R540::c_R540(){
}
c_R540* c_R540::m_new(){
	c_FieldInfo::m_new(String(L"data",4),2,bb_reflection__classes[99]);
	return this;
}
void c_R540::mark(){
	c_FieldInfo::mark();
}
c_R541::c_R541(){
}
c_R541* c_R541::m_new(){
	c_FieldInfo::m_new(String(L"length",6),2,bb_reflection__intClass);
	return this;
}
void c_R541::mark(){
	c_FieldInfo::mark();
}
c_R513::c_R513(){
}
c_R513* c_R513::m_new(){
	c_MethodInfo::m_new(String(L"ToArray",7),0,bb_reflection__classes[99],Array<c_ClassInfo* >());
	return this;
}
void c_R513::mark(){
	c_MethodInfo::mark();
}
c_R514::c_R514(){
}
c_R514* c_R514::m_new(){
	c_ClassInfo* t_[]={bb_reflection__floatClass,bb_reflection__floatClass};
	c_MethodInfo::m_new(String(L"Equals",6),0,bb_reflection__boolClass,Array<c_ClassInfo* >(t_,2));
	return this;
}
void c_R514::mark(){
	c_MethodInfo::mark();
}
c_R515::c_R515(){
}
c_R515* c_R515::m_new(){
	c_ClassInfo* t_[]={bb_reflection__floatClass,bb_reflection__floatClass};
	c_MethodInfo::m_new(String(L"Compare",7),0,bb_reflection__intClass,Array<c_ClassInfo* >(t_,2));
	return this;
}
void c_R515::mark(){
	c_MethodInfo::mark();
}
c_R516::c_R516(){
}
c_R516* c_R516::m_new(){
	c_MethodInfo::m_new(String(L"Clear",5),0,0,Array<c_ClassInfo* >());
	return this;
}
void c_R516::mark(){
	c_MethodInfo::mark();
}
c_R517::c_R517(){
}
c_R517* c_R517::m_new(){
	c_ClassInfo* t_[]={bb_reflection__intClass};
	c_MethodInfo::m_new(String(L"Length",6),0,0,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R517::mark(){
	c_MethodInfo::mark();
}
c_R518::c_R518(){
}
c_R518* c_R518::m_new(){
	c_MethodInfo::m_new(String(L"Length",6),0,bb_reflection__intClass,Array<c_ClassInfo* >());
	return this;
}
void c_R518::mark(){
	c_MethodInfo::mark();
}
c_R519::c_R519(){
}
c_R519* c_R519::m_new(){
	c_MethodInfo::m_new(String(L"IsEmpty",7),0,bb_reflection__boolClass,Array<c_ClassInfo* >());
	return this;
}
void c_R519::mark(){
	c_MethodInfo::mark();
}
c_R520::c_R520(){
}
c_R520* c_R520::m_new(){
	c_ClassInfo* t_[]={bb_reflection__floatClass};
	c_MethodInfo::m_new(String(L"Contains",8),0,bb_reflection__boolClass,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R520::mark(){
	c_MethodInfo::mark();
}
c_R521::c_R521(){
}
c_R521* c_R521::m_new(){
	c_ClassInfo* t_[]={bb_reflection__floatClass};
	c_MethodInfo::m_new(String(L"Push",4),0,0,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R521::mark(){
	c_MethodInfo::mark();
}
c_R522::c_R522(){
}
c_R522* c_R522::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[99],bb_reflection__intClass};
	c_MethodInfo::m_new(String(L"Push",4),0,0,Array<c_ClassInfo* >(t_,2));
	return this;
}
void c_R522::mark(){
	c_MethodInfo::mark();
}
c_R523::c_R523(){
}
c_R523* c_R523::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[99],bb_reflection__intClass,bb_reflection__intClass};
	c_MethodInfo::m_new(String(L"Push",4),0,0,Array<c_ClassInfo* >(t_,3));
	return this;
}
void c_R523::mark(){
	c_MethodInfo::mark();
}
c_R524::c_R524(){
}
c_R524* c_R524::m_new(){
	c_MethodInfo::m_new(String(L"Pop",3),0,bb_reflection__floatClass,Array<c_ClassInfo* >());
	return this;
}
void c_R524::mark(){
	c_MethodInfo::mark();
}
c_R525::c_R525(){
}
c_R525* c_R525::m_new(){
	c_MethodInfo::m_new(String(L"Top",3),0,bb_reflection__floatClass,Array<c_ClassInfo* >());
	return this;
}
void c_R525::mark(){
	c_MethodInfo::mark();
}
c_R526::c_R526(){
}
c_R526* c_R526::m_new(){
	c_ClassInfo* t_[]={bb_reflection__intClass,bb_reflection__floatClass};
	c_MethodInfo::m_new(String(L"Set",3),0,0,Array<c_ClassInfo* >(t_,2));
	return this;
}
void c_R526::mark(){
	c_MethodInfo::mark();
}
c_R527::c_R527(){
}
c_R527* c_R527::m_new(){
	c_ClassInfo* t_[]={bb_reflection__intClass};
	c_MethodInfo::m_new(String(L"Get",3),0,bb_reflection__floatClass,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R527::mark(){
	c_MethodInfo::mark();
}
c_R528::c_R528(){
}
c_R528* c_R528::m_new(){
	c_ClassInfo* t_[]={bb_reflection__floatClass,bb_reflection__intClass};
	c_MethodInfo::m_new(String(L"Find",4),0,bb_reflection__intClass,Array<c_ClassInfo* >(t_,2));
	return this;
}
void c_R528::mark(){
	c_MethodInfo::mark();
}
c_R529::c_R529(){
}
c_R529* c_R529::m_new(){
	c_ClassInfo* t_[]={bb_reflection__floatClass};
	c_MethodInfo::m_new(String(L"FindLast",8),0,bb_reflection__intClass,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R529::mark(){
	c_MethodInfo::mark();
}
c_R530::c_R530(){
}
c_R530* c_R530::m_new(){
	c_ClassInfo* t_[]={bb_reflection__floatClass,bb_reflection__intClass};
	c_MethodInfo::m_new(String(L"FindLast",8),0,bb_reflection__intClass,Array<c_ClassInfo* >(t_,2));
	return this;
}
void c_R530::mark(){
	c_MethodInfo::mark();
}
c_R531::c_R531(){
}
c_R531* c_R531::m_new(){
	c_ClassInfo* t_[]={bb_reflection__intClass,bb_reflection__floatClass};
	c_MethodInfo::m_new(String(L"Insert",6),0,0,Array<c_ClassInfo* >(t_,2));
	return this;
}
void c_R531::mark(){
	c_MethodInfo::mark();
}
c_R532::c_R532(){
}
c_R532* c_R532::m_new(){
	c_ClassInfo* t_[]={bb_reflection__intClass};
	c_MethodInfo::m_new(String(L"Remove",6),0,0,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R532::mark(){
	c_MethodInfo::mark();
}
c_R533::c_R533(){
}
c_R533* c_R533::m_new(){
	c_ClassInfo* t_[]={bb_reflection__floatClass};
	c_MethodInfo::m_new(String(L"RemoveFirst",11),0,0,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R533::mark(){
	c_MethodInfo::mark();
}
c_R534::c_R534(){
}
c_R534* c_R534::m_new(){
	c_ClassInfo* t_[]={bb_reflection__floatClass};
	c_MethodInfo::m_new(String(L"RemoveLast",10),0,0,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R534::mark(){
	c_MethodInfo::mark();
}
c_R535::c_R535(){
}
c_R535* c_R535::m_new(){
	c_ClassInfo* t_[]={bb_reflection__floatClass};
	c_MethodInfo::m_new(String(L"RemoveEach",10),0,0,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R535::mark(){
	c_MethodInfo::mark();
}
c_R536::c_R536(){
}
c_R536* c_R536::m_new(){
	c_ClassInfo* t_[]={bb_reflection__boolClass};
	c_MethodInfo::m_new(String(L"Sort",4),0,0,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R536::mark(){
	c_MethodInfo::mark();
}
c_R537::c_R537(){
}
c_R537* c_R537::m_new(){
	c_MethodInfo::m_new(String(L"ObjectEnumerator",16),0,bb_reflection__classes[91],Array<c_ClassInfo* >());
	return this;
}
void c_R537::mark(){
	c_MethodInfo::mark();
}
c_R538::c_R538(){
}
c_R538* c_R538::m_new(){
	c_MethodInfo::m_new(String(L"Backwards",9),0,bb_reflection__classes[92],Array<c_ClassInfo* >());
	return this;
}
void c_R538::mark(){
	c_MethodInfo::mark();
}
c_R542::c_R542(){
}
c_R542* c_R542::m_new(){
	c_ClassInfo* t_[]={bb_reflection__intClass,bb_reflection__intClass};
	c_MethodInfo::m_new(String(L"_Swap",5),8,0,Array<c_ClassInfo* >(t_,2));
	return this;
}
void c_R542::mark(){
	c_MethodInfo::mark();
}
c_R543::c_R543(){
}
c_R543* c_R543::m_new(){
	c_ClassInfo* t_[]={bb_reflection__intClass,bb_reflection__intClass,bb_reflection__intClass};
	c_MethodInfo::m_new(String(L"_Less",5),8,bb_reflection__boolClass,Array<c_ClassInfo* >(t_,3));
	return this;
}
void c_R543::mark(){
	c_MethodInfo::mark();
}
c_R544::c_R544(){
}
c_R544* c_R544::m_new(){
	c_ClassInfo* t_[]={bb_reflection__intClass,bb_reflection__floatClass,bb_reflection__intClass};
	c_MethodInfo::m_new(String(L"_Less2",6),8,bb_reflection__boolClass,Array<c_ClassInfo* >(t_,3));
	return this;
}
void c_R544::mark(){
	c_MethodInfo::mark();
}
c_R545::c_R545(){
}
c_R545* c_R545::m_new(){
	c_ClassInfo* t_[]={bb_reflection__floatClass,bb_reflection__intClass,bb_reflection__intClass};
	c_MethodInfo::m_new(String(L"_Less3",6),8,bb_reflection__boolClass,Array<c_ClassInfo* >(t_,3));
	return this;
}
void c_R545::mark(){
	c_MethodInfo::mark();
}
c_R546::c_R546(){
}
c_R546* c_R546::m_new(){
	c_ClassInfo* t_[]={bb_reflection__intClass,bb_reflection__intClass,bb_reflection__intClass};
	c_MethodInfo::m_new(String(L"_Sort",5),8,0,Array<c_ClassInfo* >(t_,3));
	return this;
}
void c_R546::mark(){
	c_MethodInfo::mark();
}
c_R511::c_R511(){
}
c_R511* c_R511::m_new(){
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[39],Array<c_ClassInfo* >());
	return this;
}
void c_R511::mark(){
	c_FunctionInfo::mark();
}
c_R512::c_R512(){
}
c_R512* c_R512::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[99]};
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[39],Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R512::mark(){
	c_FunctionInfo::mark();
}
c_R549::c_R549(){
}
c_R549* c_R549::m_new(){
	c_ClassInfo* t_[]={bb_reflection__floatClass,bb_reflection__floatClass};
	c_MethodInfo::m_new(String(L"Equals",6),0,bb_reflection__boolClass,Array<c_ClassInfo* >(t_,2));
	return this;
}
void c_R549::mark(){
	c_MethodInfo::mark();
}
c_R550::c_R550(){
}
c_R550* c_R550::m_new(){
	c_ClassInfo* t_[]={bb_reflection__floatClass,bb_reflection__floatClass};
	c_MethodInfo::m_new(String(L"Compare",7),0,bb_reflection__intClass,Array<c_ClassInfo* >(t_,2));
	return this;
}
void c_R550::mark(){
	c_MethodInfo::mark();
}
c_R548::c_R548(){
}
c_R548* c_R548::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[99]};
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[40],Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R548::mark(){
	c_FunctionInfo::mark();
}
c_R551::c_R551(){
}
c_R551* c_R551::m_new(){
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[40],Array<c_ClassInfo* >());
	return this;
}
void c_R551::mark(){
	c_FunctionInfo::mark();
}
c_R581::c_R581(){
}
c_R581* c_R581::m_new(){
	c_GlobalInfo::m_new(String(L"NIL",3),2,bb_reflection__stringClass);
	return this;
}
void c_R581::mark(){
	c_GlobalInfo::mark();
}
c_R582::c_R582(){
}
c_R582* c_R582::m_new(){
	c_FieldInfo::m_new(String(L"data",4),2,bb_reflection__classes[100]);
	return this;
}
void c_R582::mark(){
	c_FieldInfo::mark();
}
c_R583::c_R583(){
}
c_R583* c_R583::m_new(){
	c_FieldInfo::m_new(String(L"length",6),2,bb_reflection__intClass);
	return this;
}
void c_R583::mark(){
	c_FieldInfo::mark();
}
c_R555::c_R555(){
}
c_R555* c_R555::m_new(){
	c_MethodInfo::m_new(String(L"ToArray",7),0,bb_reflection__classes[100],Array<c_ClassInfo* >());
	return this;
}
void c_R555::mark(){
	c_MethodInfo::mark();
}
c_R556::c_R556(){
}
c_R556* c_R556::m_new(){
	c_ClassInfo* t_[]={bb_reflection__stringClass,bb_reflection__stringClass};
	c_MethodInfo::m_new(String(L"Equals",6),0,bb_reflection__boolClass,Array<c_ClassInfo* >(t_,2));
	return this;
}
void c_R556::mark(){
	c_MethodInfo::mark();
}
c_R557::c_R557(){
}
c_R557* c_R557::m_new(){
	c_ClassInfo* t_[]={bb_reflection__stringClass,bb_reflection__stringClass};
	c_MethodInfo::m_new(String(L"Compare",7),0,bb_reflection__intClass,Array<c_ClassInfo* >(t_,2));
	return this;
}
void c_R557::mark(){
	c_MethodInfo::mark();
}
c_R558::c_R558(){
}
c_R558* c_R558::m_new(){
	c_MethodInfo::m_new(String(L"Clear",5),0,0,Array<c_ClassInfo* >());
	return this;
}
void c_R558::mark(){
	c_MethodInfo::mark();
}
c_R559::c_R559(){
}
c_R559* c_R559::m_new(){
	c_ClassInfo* t_[]={bb_reflection__intClass};
	c_MethodInfo::m_new(String(L"Length",6),0,0,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R559::mark(){
	c_MethodInfo::mark();
}
c_R560::c_R560(){
}
c_R560* c_R560::m_new(){
	c_MethodInfo::m_new(String(L"Length",6),0,bb_reflection__intClass,Array<c_ClassInfo* >());
	return this;
}
void c_R560::mark(){
	c_MethodInfo::mark();
}
c_R561::c_R561(){
}
c_R561* c_R561::m_new(){
	c_MethodInfo::m_new(String(L"IsEmpty",7),0,bb_reflection__boolClass,Array<c_ClassInfo* >());
	return this;
}
void c_R561::mark(){
	c_MethodInfo::mark();
}
c_R562::c_R562(){
}
c_R562* c_R562::m_new(){
	c_ClassInfo* t_[]={bb_reflection__stringClass};
	c_MethodInfo::m_new(String(L"Contains",8),0,bb_reflection__boolClass,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R562::mark(){
	c_MethodInfo::mark();
}
c_R563::c_R563(){
}
c_R563* c_R563::m_new(){
	c_ClassInfo* t_[]={bb_reflection__stringClass};
	c_MethodInfo::m_new(String(L"Push",4),0,0,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R563::mark(){
	c_MethodInfo::mark();
}
c_R564::c_R564(){
}
c_R564* c_R564::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[100],bb_reflection__intClass};
	c_MethodInfo::m_new(String(L"Push",4),0,0,Array<c_ClassInfo* >(t_,2));
	return this;
}
void c_R564::mark(){
	c_MethodInfo::mark();
}
c_R565::c_R565(){
}
c_R565* c_R565::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[100],bb_reflection__intClass,bb_reflection__intClass};
	c_MethodInfo::m_new(String(L"Push",4),0,0,Array<c_ClassInfo* >(t_,3));
	return this;
}
void c_R565::mark(){
	c_MethodInfo::mark();
}
c_R566::c_R566(){
}
c_R566* c_R566::m_new(){
	c_MethodInfo::m_new(String(L"Pop",3),0,bb_reflection__stringClass,Array<c_ClassInfo* >());
	return this;
}
void c_R566::mark(){
	c_MethodInfo::mark();
}
c_R567::c_R567(){
}
c_R567* c_R567::m_new(){
	c_MethodInfo::m_new(String(L"Top",3),0,bb_reflection__stringClass,Array<c_ClassInfo* >());
	return this;
}
void c_R567::mark(){
	c_MethodInfo::mark();
}
c_R568::c_R568(){
}
c_R568* c_R568::m_new(){
	c_ClassInfo* t_[]={bb_reflection__intClass,bb_reflection__stringClass};
	c_MethodInfo::m_new(String(L"Set",3),0,0,Array<c_ClassInfo* >(t_,2));
	return this;
}
void c_R568::mark(){
	c_MethodInfo::mark();
}
c_R569::c_R569(){
}
c_R569* c_R569::m_new(){
	c_ClassInfo* t_[]={bb_reflection__intClass};
	c_MethodInfo::m_new(String(L"Get",3),0,bb_reflection__stringClass,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R569::mark(){
	c_MethodInfo::mark();
}
c_R570::c_R570(){
}
c_R570* c_R570::m_new(){
	c_ClassInfo* t_[]={bb_reflection__stringClass,bb_reflection__intClass};
	c_MethodInfo::m_new(String(L"Find",4),0,bb_reflection__intClass,Array<c_ClassInfo* >(t_,2));
	return this;
}
void c_R570::mark(){
	c_MethodInfo::mark();
}
c_R571::c_R571(){
}
c_R571* c_R571::m_new(){
	c_ClassInfo* t_[]={bb_reflection__stringClass};
	c_MethodInfo::m_new(String(L"FindLast",8),0,bb_reflection__intClass,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R571::mark(){
	c_MethodInfo::mark();
}
c_R572::c_R572(){
}
c_R572* c_R572::m_new(){
	c_ClassInfo* t_[]={bb_reflection__stringClass,bb_reflection__intClass};
	c_MethodInfo::m_new(String(L"FindLast",8),0,bb_reflection__intClass,Array<c_ClassInfo* >(t_,2));
	return this;
}
void c_R572::mark(){
	c_MethodInfo::mark();
}
c_R573::c_R573(){
}
c_R573* c_R573::m_new(){
	c_ClassInfo* t_[]={bb_reflection__intClass,bb_reflection__stringClass};
	c_MethodInfo::m_new(String(L"Insert",6),0,0,Array<c_ClassInfo* >(t_,2));
	return this;
}
void c_R573::mark(){
	c_MethodInfo::mark();
}
c_R574::c_R574(){
}
c_R574* c_R574::m_new(){
	c_ClassInfo* t_[]={bb_reflection__intClass};
	c_MethodInfo::m_new(String(L"Remove",6),0,0,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R574::mark(){
	c_MethodInfo::mark();
}
c_R575::c_R575(){
}
c_R575* c_R575::m_new(){
	c_ClassInfo* t_[]={bb_reflection__stringClass};
	c_MethodInfo::m_new(String(L"RemoveFirst",11),0,0,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R575::mark(){
	c_MethodInfo::mark();
}
c_R576::c_R576(){
}
c_R576* c_R576::m_new(){
	c_ClassInfo* t_[]={bb_reflection__stringClass};
	c_MethodInfo::m_new(String(L"RemoveLast",10),0,0,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R576::mark(){
	c_MethodInfo::mark();
}
c_R577::c_R577(){
}
c_R577* c_R577::m_new(){
	c_ClassInfo* t_[]={bb_reflection__stringClass};
	c_MethodInfo::m_new(String(L"RemoveEach",10),0,0,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R577::mark(){
	c_MethodInfo::mark();
}
c_R578::c_R578(){
}
c_R578* c_R578::m_new(){
	c_ClassInfo* t_[]={bb_reflection__boolClass};
	c_MethodInfo::m_new(String(L"Sort",4),0,0,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R578::mark(){
	c_MethodInfo::mark();
}
c_R579::c_R579(){
}
c_R579* c_R579::m_new(){
	c_MethodInfo::m_new(String(L"ObjectEnumerator",16),0,bb_reflection__classes[93],Array<c_ClassInfo* >());
	return this;
}
void c_R579::mark(){
	c_MethodInfo::mark();
}
c_R580::c_R580(){
}
c_R580* c_R580::m_new(){
	c_MethodInfo::m_new(String(L"Backwards",9),0,bb_reflection__classes[94],Array<c_ClassInfo* >());
	return this;
}
void c_R580::mark(){
	c_MethodInfo::mark();
}
c_R584::c_R584(){
}
c_R584* c_R584::m_new(){
	c_ClassInfo* t_[]={bb_reflection__intClass,bb_reflection__intClass};
	c_MethodInfo::m_new(String(L"_Swap",5),8,0,Array<c_ClassInfo* >(t_,2));
	return this;
}
void c_R584::mark(){
	c_MethodInfo::mark();
}
c_R585::c_R585(){
}
c_R585* c_R585::m_new(){
	c_ClassInfo* t_[]={bb_reflection__intClass,bb_reflection__intClass,bb_reflection__intClass};
	c_MethodInfo::m_new(String(L"_Less",5),8,bb_reflection__boolClass,Array<c_ClassInfo* >(t_,3));
	return this;
}
void c_R585::mark(){
	c_MethodInfo::mark();
}
c_R586::c_R586(){
}
c_R586* c_R586::m_new(){
	c_ClassInfo* t_[]={bb_reflection__intClass,bb_reflection__stringClass,bb_reflection__intClass};
	c_MethodInfo::m_new(String(L"_Less2",6),8,bb_reflection__boolClass,Array<c_ClassInfo* >(t_,3));
	return this;
}
void c_R586::mark(){
	c_MethodInfo::mark();
}
c_R587::c_R587(){
}
c_R587* c_R587::m_new(){
	c_ClassInfo* t_[]={bb_reflection__stringClass,bb_reflection__intClass,bb_reflection__intClass};
	c_MethodInfo::m_new(String(L"_Less3",6),8,bb_reflection__boolClass,Array<c_ClassInfo* >(t_,3));
	return this;
}
void c_R587::mark(){
	c_MethodInfo::mark();
}
c_R588::c_R588(){
}
c_R588* c_R588::m_new(){
	c_ClassInfo* t_[]={bb_reflection__intClass,bb_reflection__intClass,bb_reflection__intClass};
	c_MethodInfo::m_new(String(L"_Sort",5),8,0,Array<c_ClassInfo* >(t_,3));
	return this;
}
void c_R588::mark(){
	c_MethodInfo::mark();
}
c_R553::c_R553(){
}
c_R553* c_R553::m_new(){
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[41],Array<c_ClassInfo* >());
	return this;
}
void c_R553::mark(){
	c_FunctionInfo::mark();
}
c_R554::c_R554(){
}
c_R554* c_R554::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[100]};
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[41],Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R554::mark(){
	c_FunctionInfo::mark();
}
c_R591::c_R591(){
}
c_R591* c_R591::m_new(){
	c_ClassInfo* t_[]={bb_reflection__stringClass};
	c_MethodInfo::m_new(String(L"Join",4),0,bb_reflection__stringClass,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R591::mark(){
	c_MethodInfo::mark();
}
c_R592::c_R592(){
}
c_R592* c_R592::m_new(){
	c_ClassInfo* t_[]={bb_reflection__stringClass,bb_reflection__stringClass};
	c_MethodInfo::m_new(String(L"Equals",6),0,bb_reflection__boolClass,Array<c_ClassInfo* >(t_,2));
	return this;
}
void c_R592::mark(){
	c_MethodInfo::mark();
}
c_R593::c_R593(){
}
c_R593* c_R593::m_new(){
	c_ClassInfo* t_[]={bb_reflection__stringClass,bb_reflection__stringClass};
	c_MethodInfo::m_new(String(L"Compare",7),0,bb_reflection__intClass,Array<c_ClassInfo* >(t_,2));
	return this;
}
void c_R593::mark(){
	c_MethodInfo::mark();
}
c_R590::c_R590(){
}
c_R590* c_R590::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[100]};
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[42],Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R590::mark(){
	c_FunctionInfo::mark();
}
c_R594::c_R594(){
}
c_R594* c_R594::m_new(){
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[42],Array<c_ClassInfo* >());
	return this;
}
void c_R594::mark(){
	c_FunctionInfo::mark();
}
c_R596::c_R596(){
}
c_R596* c_R596::m_new(){
	c_FieldInfo::m_new(String(L"Width",5),1,bb_reflection__intClass);
	return this;
}
void c_R596::mark(){
	c_FieldInfo::mark();
}
c_R597::c_R597(){
}
c_R597* c_R597::m_new(){
	c_FieldInfo::m_new(String(L"Height",6),1,bb_reflection__intClass);
	return this;
}
void c_R597::mark(){
	c_FieldInfo::mark();
}
c_R598::c_R598(){
}
c_R598* c_R598::m_new(){
	c_FieldInfo::m_new(String(L"RedBits",7),1,bb_reflection__intClass);
	return this;
}
void c_R598::mark(){
	c_FieldInfo::mark();
}
c_R599::c_R599(){
}
c_R599* c_R599::m_new(){
	c_FieldInfo::m_new(String(L"GreenBits",9),1,bb_reflection__intClass);
	return this;
}
void c_R599::mark(){
	c_FieldInfo::mark();
}
c_R600::c_R600(){
}
c_R600* c_R600::m_new(){
	c_FieldInfo::m_new(String(L"BlueBits",8),1,bb_reflection__intClass);
	return this;
}
void c_R600::mark(){
	c_FieldInfo::mark();
}
c_R606::c_R606(){
}
c_R606* c_R606::m_new(){
	c_GlobalInfo::m_new(String(L"Black",5),0,bb_reflection__classes[45]);
	return this;
}
void c_R606::mark(){
	c_GlobalInfo::mark();
}
c_R607::c_R607(){
}
c_R607* c_R607::m_new(){
	c_GlobalInfo::m_new(String(L"White",5),0,bb_reflection__classes[45]);
	return this;
}
void c_R607::mark(){
	c_GlobalInfo::mark();
}
c_R608::c_R608(){
}
c_R608* c_R608::m_new(){
	c_GlobalInfo::m_new(String(L"PureRed",7),0,bb_reflection__classes[45]);
	return this;
}
void c_R608::mark(){
	c_GlobalInfo::mark();
}
c_R609::c_R609(){
}
c_R609* c_R609::m_new(){
	c_GlobalInfo::m_new(String(L"PureGreen",9),0,bb_reflection__classes[45]);
	return this;
}
void c_R609::mark(){
	c_GlobalInfo::mark();
}
c_R610::c_R610(){
}
c_R610* c_R610::m_new(){
	c_GlobalInfo::m_new(String(L"PureBlue",8),0,bb_reflection__classes[45]);
	return this;
}
void c_R610::mark(){
	c_GlobalInfo::mark();
}
c_R611::c_R611(){
}
c_R611* c_R611::m_new(){
	c_GlobalInfo::m_new(String(L"Navy",4),0,bb_reflection__classes[45]);
	return this;
}
void c_R611::mark(){
	c_GlobalInfo::mark();
}
c_R612::c_R612(){
}
c_R612* c_R612::m_new(){
	c_GlobalInfo::m_new(String(L"NewBlue",7),0,bb_reflection__classes[45]);
	return this;
}
void c_R612::mark(){
	c_GlobalInfo::mark();
}
c_R613::c_R613(){
}
c_R613* c_R613::m_new(){
	c_GlobalInfo::m_new(String(L"Aqua",4),0,bb_reflection__classes[45]);
	return this;
}
void c_R613::mark(){
	c_GlobalInfo::mark();
}
c_R614::c_R614(){
}
c_R614* c_R614::m_new(){
	c_GlobalInfo::m_new(String(L"Teal",4),0,bb_reflection__classes[45]);
	return this;
}
void c_R614::mark(){
	c_GlobalInfo::mark();
}
c_R615::c_R615(){
}
c_R615* c_R615::m_new(){
	c_GlobalInfo::m_new(String(L"Olive",5),0,bb_reflection__classes[45]);
	return this;
}
void c_R615::mark(){
	c_GlobalInfo::mark();
}
c_R616::c_R616(){
}
c_R616* c_R616::m_new(){
	c_GlobalInfo::m_new(String(L"NewGreen",8),0,bb_reflection__classes[45]);
	return this;
}
void c_R616::mark(){
	c_GlobalInfo::mark();
}
c_R617::c_R617(){
}
c_R617* c_R617::m_new(){
	c_GlobalInfo::m_new(String(L"Lime",4),0,bb_reflection__classes[45]);
	return this;
}
void c_R617::mark(){
	c_GlobalInfo::mark();
}
c_R618::c_R618(){
}
c_R618* c_R618::m_new(){
	c_GlobalInfo::m_new(String(L"Yellow",6),0,bb_reflection__classes[45]);
	return this;
}
void c_R618::mark(){
	c_GlobalInfo::mark();
}
c_R619::c_R619(){
}
c_R619* c_R619::m_new(){
	c_GlobalInfo::m_new(String(L"Orange",6),0,bb_reflection__classes[45]);
	return this;
}
void c_R619::mark(){
	c_GlobalInfo::mark();
}
c_R620::c_R620(){
}
c_R620* c_R620::m_new(){
	c_GlobalInfo::m_new(String(L"NewRed",6),0,bb_reflection__classes[45]);
	return this;
}
void c_R620::mark(){
	c_GlobalInfo::mark();
}
c_R621::c_R621(){
}
c_R621* c_R621::m_new(){
	c_GlobalInfo::m_new(String(L"Maroon",6),0,bb_reflection__classes[45]);
	return this;
}
void c_R621::mark(){
	c_GlobalInfo::mark();
}
c_R622::c_R622(){
}
c_R622* c_R622::m_new(){
	c_GlobalInfo::m_new(String(L"Fuchsia",7),0,bb_reflection__classes[45]);
	return this;
}
void c_R622::mark(){
	c_GlobalInfo::mark();
}
c_R623::c_R623(){
}
c_R623* c_R623::m_new(){
	c_GlobalInfo::m_new(String(L"Purple",6),0,bb_reflection__classes[45]);
	return this;
}
void c_R623::mark(){
	c_GlobalInfo::mark();
}
c_R624::c_R624(){
}
c_R624* c_R624::m_new(){
	c_GlobalInfo::m_new(String(L"Silver",6),0,bb_reflection__classes[45]);
	return this;
}
void c_R624::mark(){
	c_GlobalInfo::mark();
}
c_R625::c_R625(){
}
c_R625* c_R625::m_new(){
	c_GlobalInfo::m_new(String(L"Gray",4),0,bb_reflection__classes[45]);
	return this;
}
void c_R625::mark(){
	c_GlobalInfo::mark();
}
c_R626::c_R626(){
}
c_R626* c_R626::m_new(){
	c_GlobalInfo::m_new(String(L"NewBlack",8),0,bb_reflection__classes[45]);
	return this;
}
void c_R626::mark(){
	c_GlobalInfo::mark();
}
c_R602::c_R602(){
}
c_R602* c_R602::m_new(){
	c_FieldInfo::m_new(String(L"red",3),2,bb_reflection__floatClass);
	return this;
}
void c_R602::mark(){
	c_FieldInfo::mark();
}
c_R603::c_R603(){
}
c_R603* c_R603::m_new(){
	c_FieldInfo::m_new(String(L"green",5),2,bb_reflection__floatClass);
	return this;
}
void c_R603::mark(){
	c_FieldInfo::mark();
}
c_R604::c_R604(){
}
c_R604* c_R604::m_new(){
	c_FieldInfo::m_new(String(L"blue",4),2,bb_reflection__floatClass);
	return this;
}
void c_R604::mark(){
	c_FieldInfo::mark();
}
c_R605::c_R605(){
}
c_R605* c_R605::m_new(){
	c_FieldInfo::m_new(String(L"alpha",5),2,bb_reflection__floatClass);
	return this;
}
void c_R605::mark(){
	c_FieldInfo::mark();
}
c_R630::c_R630(){
}
c_R630* c_R630::m_new(){
	c_ClassInfo* t_[]={bb_reflection__floatClass,bb_reflection__floatClass,bb_reflection__floatClass,bb_reflection__floatClass};
	c_MethodInfo::m_new(String(L"Set",3),0,0,Array<c_ClassInfo* >(t_,4));
	return this;
}
void c_R630::mark(){
	c_MethodInfo::mark();
}
c_R631::c_R631(){
}
c_R631* c_R631::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[44]};
	c_MethodInfo::m_new(String(L"Set",3),0,0,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R631::mark(){
	c_MethodInfo::mark();
}
c_R632::c_R632(){
}
c_R632* c_R632::m_new(){
	c_ClassInfo* t_[]={bb_reflection__intClass};
	c_MethodInfo::m_new(String(L"Set",3),0,0,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R632::mark(){
	c_MethodInfo::mark();
}
c_R633::c_R633(){
}
c_R633* c_R633::m_new(){
	c_MethodInfo::m_new(String(L"Reset",5),0,0,Array<c_ClassInfo* >());
	return this;
}
void c_R633::mark(){
	c_MethodInfo::mark();
}
c_R634::c_R634(){
}
c_R634* c_R634::m_new(){
	c_MethodInfo::m_new(String(L"Red",3),0,bb_reflection__floatClass,Array<c_ClassInfo* >());
	return this;
}
void c_R634::mark(){
	c_MethodInfo::mark();
}
c_R635::c_R635(){
}
c_R635* c_R635::m_new(){
	c_ClassInfo* t_[]={bb_reflection__floatClass};
	c_MethodInfo::m_new(String(L"Red",3),0,0,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R635::mark(){
	c_MethodInfo::mark();
}
c_R636::c_R636(){
}
c_R636* c_R636::m_new(){
	c_MethodInfo::m_new(String(L"Green",5),0,bb_reflection__floatClass,Array<c_ClassInfo* >());
	return this;
}
void c_R636::mark(){
	c_MethodInfo::mark();
}
c_R637::c_R637(){
}
c_R637* c_R637::m_new(){
	c_ClassInfo* t_[]={bb_reflection__floatClass};
	c_MethodInfo::m_new(String(L"Green",5),0,0,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R637::mark(){
	c_MethodInfo::mark();
}
c_R638::c_R638(){
}
c_R638* c_R638::m_new(){
	c_MethodInfo::m_new(String(L"Blue",4),0,bb_reflection__floatClass,Array<c_ClassInfo* >());
	return this;
}
void c_R638::mark(){
	c_MethodInfo::mark();
}
c_R639::c_R639(){
}
c_R639* c_R639::m_new(){
	c_ClassInfo* t_[]={bb_reflection__floatClass};
	c_MethodInfo::m_new(String(L"Blue",4),0,0,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R639::mark(){
	c_MethodInfo::mark();
}
c_R640::c_R640(){
}
c_R640* c_R640::m_new(){
	c_MethodInfo::m_new(String(L"Alpha",5),0,bb_reflection__floatClass,Array<c_ClassInfo* >());
	return this;
}
void c_R640::mark(){
	c_MethodInfo::mark();
}
c_R641::c_R641(){
}
c_R641* c_R641::m_new(){
	c_ClassInfo* t_[]={bb_reflection__floatClass};
	c_MethodInfo::m_new(String(L"Alpha",5),0,0,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R641::mark(){
	c_MethodInfo::mark();
}
c_R642::c_R642(){
}
c_R642* c_R642::m_new(){
	c_ClassInfo* t_[]={bb_reflection__intClass};
	c_MethodInfo::m_new(String(L"RGB",3),0,0,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R642::mark(){
	c_MethodInfo::mark();
}
c_R643::c_R643(){
}
c_R643* c_R643::m_new(){
	c_MethodInfo::m_new(String(L"RGB",3),0,bb_reflection__intClass,Array<c_ClassInfo* >());
	return this;
}
void c_R643::mark(){
	c_MethodInfo::mark();
}
c_R644::c_R644(){
}
c_R644* c_R644::m_new(){
	c_MethodInfo::m_new(String(L"ARGB",4),0,bb_reflection__intClass,Array<c_ClassInfo* >());
	return this;
}
void c_R644::mark(){
	c_MethodInfo::mark();
}
c_R645::c_R645(){
}
c_R645* c_R645::m_new(){
	c_MethodInfo::m_new(String(L"Randomize",9),0,0,Array<c_ClassInfo* >());
	return this;
}
void c_R645::mark(){
	c_MethodInfo::mark();
}
c_R646::c_R646(){
}
c_R646* c_R646::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[44]};
	c_MethodInfo::m_new(String(L"Equals",6),0,bb_reflection__boolClass,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R646::mark(){
	c_MethodInfo::mark();
}
c_R647::c_R647(){
}
c_R647* c_R647::m_new(){
	c_MethodInfo::m_new(String(L"ToString",8),0,bb_reflection__stringClass,Array<c_ClassInfo* >());
	return this;
}
void c_R647::mark(){
	c_MethodInfo::mark();
}
c_R648::c_R648(){
}
c_R648* c_R648::m_new(){
	c_MethodInfo::m_new(String(L"Use",3),0,0,Array<c_ClassInfo* >());
	return this;
}
void c_R648::mark(){
	c_MethodInfo::mark();
}
c_R649::c_R649(){
}
c_R649* c_R649::m_new(){
	c_MethodInfo::m_new(String(L"UseWithoutAlpha",15),0,0,Array<c_ClassInfo* >());
	return this;
}
void c_R649::mark(){
	c_MethodInfo::mark();
}
c_R627::c_R627(){
}
c_R627* c_R627::m_new(){
	c_ClassInfo* t_[]={bb_reflection__intClass};
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[44],Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R627::mark(){
	c_FunctionInfo::mark();
}
c_R628::c_R628(){
}
c_R628* c_R628::m_new(){
	c_ClassInfo* t_[]={bb_reflection__floatClass,bb_reflection__floatClass,bb_reflection__floatClass,bb_reflection__floatClass};
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[44],Array<c_ClassInfo* >(t_,4));
	return this;
}
void c_R628::mark(){
	c_FunctionInfo::mark();
}
c_R629::c_R629(){
}
c_R629* c_R629::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[44]};
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[44],Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R629::mark(){
	c_FunctionInfo::mark();
}
c_R650::c_R650(){
}
c_R650* c_R650::m_new(){
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[44],Array<c_ClassInfo* >());
	return this;
}
void c_R650::mark(){
	c_FunctionInfo::mark();
}
c_R655::c_R655(){
}
c_R655* c_R655::m_new(){
	c_ClassInfo* t_[]={bb_reflection__floatClass,bb_reflection__floatClass,bb_reflection__floatClass,bb_reflection__floatClass};
	c_MethodInfo::m_new(String(L"Set",3),0,0,Array<c_ClassInfo* >(t_,4));
	return this;
}
void c_R655::mark(){
	c_MethodInfo::mark();
}
c_R656::c_R656(){
}
c_R656* c_R656::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[44]};
	c_MethodInfo::m_new(String(L"Set",3),0,0,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R656::mark(){
	c_MethodInfo::mark();
}
c_R657::c_R657(){
}
c_R657* c_R657::m_new(){
	c_MethodInfo::m_new(String(L"Reset",5),0,0,Array<c_ClassInfo* >());
	return this;
}
void c_R657::mark(){
	c_MethodInfo::mark();
}
c_R658::c_R658(){
}
c_R658* c_R658::m_new(){
	c_MethodInfo::m_new(String(L"Randomize",9),0,0,Array<c_ClassInfo* >());
	return this;
}
void c_R658::mark(){
	c_MethodInfo::mark();
}
c_R659::c_R659(){
}
c_R659* c_R659::m_new(){
	c_ClassInfo* t_[]={bb_reflection__floatClass};
	c_MethodInfo::m_new(String(L"Red",3),0,0,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R659::mark(){
	c_MethodInfo::mark();
}
c_R660::c_R660(){
}
c_R660* c_R660::m_new(){
	c_ClassInfo* t_[]={bb_reflection__floatClass};
	c_MethodInfo::m_new(String(L"Green",5),0,0,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R660::mark(){
	c_MethodInfo::mark();
}
c_R661::c_R661(){
}
c_R661* c_R661::m_new(){
	c_ClassInfo* t_[]={bb_reflection__floatClass};
	c_MethodInfo::m_new(String(L"Blue",4),0,0,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R661::mark(){
	c_MethodInfo::mark();
}
c_R662::c_R662(){
}
c_R662* c_R662::m_new(){
	c_ClassInfo* t_[]={bb_reflection__floatClass};
	c_MethodInfo::m_new(String(L"Alpha",5),0,0,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R662::mark(){
	c_MethodInfo::mark();
}
c_R663::c_R663(){
}
c_R663* c_R663::m_new(){
	c_ClassInfo* t_[]={bb_reflection__intClass};
	c_MethodInfo::m_new(String(L"RGB",3),0,0,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R663::mark(){
	c_MethodInfo::mark();
}
c_R664::c_R664(){
}
c_R664* c_R664::m_new(){
	c_MethodInfo::m_new(String(L"CantChangeError",15),0,0,Array<c_ClassInfo* >());
	return this;
}
void c_R664::mark(){
	c_MethodInfo::mark();
}
c_R652::c_R652(){
}
c_R652* c_R652::m_new(){
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[45],Array<c_ClassInfo* >());
	return this;
}
void c_R652::mark(){
	c_FunctionInfo::mark();
}
c_R653::c_R653(){
}
c_R653* c_R653::m_new(){
	c_ClassInfo* t_[]={bb_reflection__intClass};
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[45],Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R653::mark(){
	c_FunctionInfo::mark();
}
c_R654::c_R654(){
}
c_R654* c_R654::m_new(){
	c_ClassInfo* t_[]={bb_reflection__floatClass,bb_reflection__floatClass,bb_reflection__floatClass,bb_reflection__floatClass};
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[45],Array<c_ClassInfo* >(t_,4));
	return this;
}
void c_R654::mark(){
	c_FunctionInfo::mark();
}
c_R666::c_R666(){
}
c_R666* c_R666::m_new(){
	c_FieldInfo::m_new(String(L"x",1),0,bb_reflection__floatClass);
	return this;
}
void c_R666::mark(){
	c_FieldInfo::mark();
}
c_R667::c_R667(){
}
c_R667* c_R667::m_new(){
	c_FieldInfo::m_new(String(L"y",1),0,bb_reflection__floatClass);
	return this;
}
void c_R667::mark(){
	c_FieldInfo::mark();
}
c_R670::c_R670(){
}
c_R670* c_R670::m_new(){
	c_MethodInfo::m_new(String(L"Copy",4),0,bb_reflection__classes[46],Array<c_ClassInfo* >());
	return this;
}
void c_R670::mark(){
	c_MethodInfo::mark();
}
c_R672::c_R672(){
}
c_R672* c_R672::m_new(){
	c_ClassInfo* t_[]={bb_reflection__floatClass,bb_reflection__floatClass};
	c_MethodInfo::m_new(String(L"Set",3),0,bb_reflection__classes[46],Array<c_ClassInfo* >(t_,2));
	return this;
}
void c_R672::mark(){
	c_MethodInfo::mark();
}
c_R673::c_R673(){
}
c_R673* c_R673::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[46]};
	c_MethodInfo::m_new(String(L"Set",3),0,bb_reflection__classes[46],Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R673::mark(){
	c_MethodInfo::mark();
}
c_R674::c_R674(){
}
c_R674* c_R674::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[46]};
	c_MethodInfo::m_new(String(L"Add",3),0,bb_reflection__classes[46],Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R674::mark(){
	c_MethodInfo::mark();
}
c_R675::c_R675(){
}
c_R675* c_R675::m_new(){
	c_ClassInfo* t_[]={bb_reflection__floatClass,bb_reflection__floatClass};
	c_MethodInfo::m_new(String(L"Add",3),0,bb_reflection__classes[46],Array<c_ClassInfo* >(t_,2));
	return this;
}
void c_R675::mark(){
	c_MethodInfo::mark();
}
c_R676::c_R676(){
}
c_R676* c_R676::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[46]};
	c_MethodInfo::m_new(String(L"Sub",3),0,bb_reflection__classes[46],Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R676::mark(){
	c_MethodInfo::mark();
}
c_R677::c_R677(){
}
c_R677* c_R677::m_new(){
	c_ClassInfo* t_[]={bb_reflection__floatClass,bb_reflection__floatClass};
	c_MethodInfo::m_new(String(L"Sub",3),0,bb_reflection__classes[46],Array<c_ClassInfo* >(t_,2));
	return this;
}
void c_R677::mark(){
	c_MethodInfo::mark();
}
c_R678::c_R678(){
}
c_R678* c_R678::m_new(){
	c_ClassInfo* t_[]={bb_reflection__floatClass};
	c_MethodInfo::m_new(String(L"Mul",3),0,bb_reflection__classes[46],Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R678::mark(){
	c_MethodInfo::mark();
}
c_R679::c_R679(){
}
c_R679* c_R679::m_new(){
	c_ClassInfo* t_[]={bb_reflection__floatClass};
	c_MethodInfo::m_new(String(L"Div",3),0,bb_reflection__classes[46],Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R679::mark(){
	c_MethodInfo::mark();
}
c_R680::c_R680(){
}
c_R680* c_R680::m_new(){
	c_MethodInfo::m_new(String(L"Normalize",9),0,bb_reflection__classes[46],Array<c_ClassInfo* >());
	return this;
}
void c_R680::mark(){
	c_MethodInfo::mark();
}
c_R681::c_R681(){
}
c_R681* c_R681::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[46]};
	c_MethodInfo::m_new(String(L"Dot",3),0,bb_reflection__floatClass,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R681::mark(){
	c_MethodInfo::mark();
}
c_R682::c_R682(){
}
c_R682* c_R682::m_new(){
	c_MethodInfo::m_new(String(L"Length",6),0,bb_reflection__floatClass,Array<c_ClassInfo* >());
	return this;
}
void c_R682::mark(){
	c_MethodInfo::mark();
}
c_R683::c_R683(){
}
c_R683* c_R683::m_new(){
	c_ClassInfo* t_[]={bb_reflection__floatClass};
	c_MethodInfo::m_new(String(L"Length",6),0,0,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R683::mark(){
	c_MethodInfo::mark();
}
c_R684::c_R684(){
}
c_R684* c_R684::m_new(){
	c_ClassInfo* t_[]={bb_reflection__floatClass};
	c_MethodInfo::m_new(String(L"Limit",5),0,0,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R684::mark(){
	c_MethodInfo::mark();
}
c_R685::c_R685(){
}
c_R685* c_R685::m_new(){
	c_MethodInfo::m_new(String(L"LengthSquared",13),0,bb_reflection__floatClass,Array<c_ClassInfo* >());
	return this;
}
void c_R685::mark(){
	c_MethodInfo::mark();
}
c_R686::c_R686(){
}
c_R686* c_R686::m_new(){
	c_MethodInfo::m_new(String(L"Angle",5),0,bb_reflection__floatClass,Array<c_ClassInfo* >());
	return this;
}
void c_R686::mark(){
	c_MethodInfo::mark();
}
c_R687::c_R687(){
}
c_R687* c_R687::m_new(){
	c_ClassInfo* t_[]={bb_reflection__floatClass};
	c_MethodInfo::m_new(String(L"Angle",5),0,0,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R687::mark(){
	c_MethodInfo::mark();
}
c_R688::c_R688(){
}
c_R688* c_R688::m_new(){
	c_MethodInfo::m_new(String(L"RotateLeft",10),0,0,Array<c_ClassInfo* >());
	return this;
}
void c_R688::mark(){
	c_MethodInfo::mark();
}
c_R689::c_R689(){
}
c_R689* c_R689::m_new(){
	c_ClassInfo* t_[]={bb_reflection__floatClass,bb_reflection__floatClass};
	c_MethodInfo::m_new(String(L"DistanceTo",10),0,bb_reflection__floatClass,Array<c_ClassInfo* >(t_,2));
	return this;
}
void c_R689::mark(){
	c_MethodInfo::mark();
}
c_R690::c_R690(){
}
c_R690* c_R690::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[46]};
	c_MethodInfo::m_new(String(L"DistanceTo",10),0,bb_reflection__floatClass,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R690::mark(){
	c_MethodInfo::mark();
}
c_R691::c_R691(){
}
c_R691* c_R691::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[46]};
	c_MethodInfo::m_new(String(L"Equals",6),0,bb_reflection__boolClass,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R691::mark(){
	c_MethodInfo::mark();
}
c_R692::c_R692(){
}
c_R692* c_R692::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[46]};
	c_MethodInfo::m_new(String(L"ProjectOn",9),0,bb_reflection__floatClass,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R692::mark(){
	c_MethodInfo::mark();
}
c_R693::c_R693(){
}
c_R693* c_R693::m_new(){
	c_MethodInfo::m_new(String(L"ToString",8),0,bb_reflection__stringClass,Array<c_ClassInfo* >());
	return this;
}
void c_R693::mark(){
	c_MethodInfo::mark();
}
c_R671::c_R671(){
}
c_R671* c_R671::m_new(){
	c_ClassInfo* t_[]={bb_reflection__floatClass,bb_reflection__floatClass};
	c_FunctionInfo::m_new(String(L"FromPolar",9),0,bb_reflection__classes[46],Array<c_ClassInfo* >(t_,2));
	return this;
}
void c_R671::mark(){
	c_FunctionInfo::mark();
}
c_R694::c_R694(){
}
c_R694* c_R694::m_new(){
	c_FunctionInfo::m_new(String(L"Up",2),0,bb_reflection__classes[46],Array<c_ClassInfo* >());
	return this;
}
void c_R694::mark(){
	c_FunctionInfo::mark();
}
c_R695::c_R695(){
}
c_R695* c_R695::m_new(){
	c_FunctionInfo::m_new(String(L"Down",4),0,bb_reflection__classes[46],Array<c_ClassInfo* >());
	return this;
}
void c_R695::mark(){
	c_FunctionInfo::mark();
}
c_R696::c_R696(){
}
c_R696* c_R696::m_new(){
	c_FunctionInfo::m_new(String(L"Left",4),0,bb_reflection__classes[46],Array<c_ClassInfo* >());
	return this;
}
void c_R696::mark(){
	c_FunctionInfo::mark();
}
c_R697::c_R697(){
}
c_R697* c_R697::m_new(){
	c_FunctionInfo::m_new(String(L"Right",5),0,bb_reflection__classes[46],Array<c_ClassInfo* >());
	return this;
}
void c_R697::mark(){
	c_FunctionInfo::mark();
}
c_R698::c_R698(){
}
c_R698* c_R698::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[46],bb_reflection__classes[46]};
	c_FunctionInfo::m_new(String(L"Add",3),0,bb_reflection__classes[46],Array<c_ClassInfo* >(t_,2));
	return this;
}
void c_R698::mark(){
	c_FunctionInfo::mark();
}
c_R699::c_R699(){
}
c_R699* c_R699::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[46],bb_reflection__classes[46]};
	c_FunctionInfo::m_new(String(L"Sub",3),0,bb_reflection__classes[46],Array<c_ClassInfo* >(t_,2));
	return this;
}
void c_R699::mark(){
	c_FunctionInfo::mark();
}
c_R700::c_R700(){
}
c_R700* c_R700::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[46],bb_reflection__floatClass};
	c_FunctionInfo::m_new(String(L"Mul",3),0,bb_reflection__classes[46],Array<c_ClassInfo* >(t_,2));
	return this;
}
void c_R700::mark(){
	c_FunctionInfo::mark();
}
c_R701::c_R701(){
}
c_R701* c_R701::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[46],bb_reflection__floatClass};
	c_FunctionInfo::m_new(String(L"Div",3),0,bb_reflection__classes[46],Array<c_ClassInfo* >(t_,2));
	return this;
}
void c_R701::mark(){
	c_FunctionInfo::mark();
}
c_R702::c_R702(){
}
c_R702* c_R702::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[46],bb_reflection__classes[46]};
	c_FunctionInfo::m_new(String(L"Dot",3),0,bb_reflection__floatClass,Array<c_ClassInfo* >(t_,2));
	return this;
}
void c_R702::mark(){
	c_FunctionInfo::mark();
}
c_R703::c_R703(){
}
c_R703* c_R703::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[46],bb_reflection__classes[46]};
	c_FunctionInfo::m_new(String(L"AngleBetween",12),0,bb_reflection__floatClass,Array<c_ClassInfo* >(t_,2));
	return this;
}
void c_R703::mark(){
	c_FunctionInfo::mark();
}
c_R668::c_R668(){
}
c_R668* c_R668::m_new(){
	c_ClassInfo* t_[]={bb_reflection__floatClass,bb_reflection__floatClass};
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[46],Array<c_ClassInfo* >(t_,2));
	return this;
}
void c_R668::mark(){
	c_FunctionInfo::mark();
}
c_R669::c_R669(){
}
c_R669* c_R669::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[46]};
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[46],Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R669::mark(){
	c_FunctionInfo::mark();
}
c_R704::c_R704(){
}
c_R704* c_R704::m_new(){
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[46],Array<c_ClassInfo* >());
	return this;
}
void c_R704::mark(){
	c_FunctionInfo::mark();
}
c_R706::c_R706(){
}
c_R706* c_R706::m_new(){
	c_FieldInfo::m_new(String(L"attributes",10),2,bb_reflection__classes[49]);
	return this;
}
void c_R706::mark(){
	c_FieldInfo::mark();
}
c_R707::c_R707(){
}
c_R707* c_R707::m_new(){
	c_FieldInfo::m_new(String(L"position",8),0,bb_reflection__classes[46]);
	return this;
}
void c_R707::mark(){
	c_FieldInfo::mark();
}
c_R708::c_R708(){
}
c_R708* c_R708::m_new(){
	c_FieldInfo::m_new(String(L"scale",5),0,bb_reflection__classes[46]);
	return this;
}
void c_R708::mark(){
	c_FieldInfo::mark();
}
c_R709::c_R709(){
}
c_R709* c_R709::m_new(){
	c_FieldInfo::m_new(String(L"rotation",8),0,bb_reflection__floatClass);
	return this;
}
void c_R709::mark(){
	c_FieldInfo::mark();
}
c_R710::c_R710(){
}
c_R710* c_R710::m_new(){
	c_ClassInfo* t_[]={bb_reflection__stringClass,bb_reflection__stringClass};
	c_MethodInfo::m_new(String(L"SetAttribute",12),0,0,Array<c_ClassInfo* >(t_,2));
	return this;
}
void c_R710::mark(){
	c_MethodInfo::mark();
}
c_R711::c_R711(){
}
c_R711* c_R711::m_new(){
	c_ClassInfo* t_[]={bb_reflection__stringClass,bb_reflection__boolClass};
	c_MethodInfo::m_new(String(L"SetAttribute",12),0,0,Array<c_ClassInfo* >(t_,2));
	return this;
}
void c_R711::mark(){
	c_MethodInfo::mark();
}
c_R712::c_R712(){
}
c_R712* c_R712::m_new(){
	c_ClassInfo* t_[]={bb_reflection__stringClass};
	c_MethodInfo::m_new(String(L"GetAttribute",12),0,bb_reflection__stringClass,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R712::mark(){
	c_MethodInfo::mark();
}
c_R713::c_R713(){
}
c_R713* c_R713::m_new(){
	c_ClassInfo* t_[]={bb_reflection__stringClass};
	c_MethodInfo::m_new(String(L"HasAttribute",12),0,bb_reflection__boolClass,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R713::mark(){
	c_MethodInfo::mark();
}
c_R714::c_R714(){
}
c_R714* c_R714::m_new(){
	c_MethodInfo::m_new(String(L"NumberOfAttributes",18),0,bb_reflection__intClass,Array<c_ClassInfo* >());
	return this;
}
void c_R714::mark(){
	c_MethodInfo::mark();
}
c_R715::c_R715(){
}
c_R715* c_R715::m_new(){
	c_MethodInfo::m_new(String(L"GetAttributeMap",15),0,bb_reflection__classes[49],Array<c_ClassInfo* >());
	return this;
}
void c_R715::mark(){
	c_MethodInfo::mark();
}
c_R716::c_R716(){
}
c_R716* c_R716::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[49]};
	c_MethodInfo::m_new(String(L"SetAttributeMap",15),0,0,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R716::mark(){
	c_MethodInfo::mark();
}
c_R717::c_R717(){
}
c_R717* c_R717::m_new(){
	c_ClassInfo* t_[]={bb_reflection__floatClass};
	c_MethodInfo::m_new(String(L"SetScale",8),0,0,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R717::mark(){
	c_MethodInfo::mark();
}
c_R718::c_R718(){
}
c_R718* c_R718::m_new(){
	c_MethodInfo::m_new(String(L"ApplyTransform",14),0,0,Array<c_ClassInfo* >());
	return this;
}
void c_R718::mark(){
	c_MethodInfo::mark();
}
c_R719::c_R719(){
}
c_R719* c_R719::m_new(){
	c_ClassInfo* t_[]={bb_reflection__floatClass};
	c_MethodInfo::m_new(String(L"Update",6),0,0,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R719::mark(){
	c_MethodInfo::mark();
}
c_R720::c_R720(){
}
c_R720* c_R720::m_new(){
	c_MethodInfo::m_new(String(L"Render",6),0,0,Array<c_ClassInfo* >());
	return this;
}
void c_R720::mark(){
	c_MethodInfo::mark();
}
c_R721::c_R721(){
}
c_R721* c_R721::m_new(){
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[47],Array<c_ClassInfo* >());
	return this;
}
void c_R721::mark(){
	c_FunctionInfo::mark();
}
c_R746::c_R746(){
}
c_R746* c_R746::m_new(){
	c_FieldInfo::m_new(String(L"root",4),2,bb_reflection__classes[50]);
	return this;
}
void c_R746::mark(){
	c_FieldInfo::mark();
}
c_R723::c_R723(){
}
c_R723* c_R723::m_new(){
	c_ClassInfo* t_[]={bb_reflection__stringClass,bb_reflection__stringClass};
	c_MethodInfo::m_new(String(L"Compare",7),4,bb_reflection__intClass,Array<c_ClassInfo* >(t_,2));
	return this;
}
void c_R723::mark(){
	c_MethodInfo::mark();
}
c_R724::c_R724(){
}
c_R724* c_R724::m_new(){
	c_MethodInfo::m_new(String(L"Clear",5),0,bb_reflection__intClass,Array<c_ClassInfo* >());
	return this;
}
void c_R724::mark(){
	c_MethodInfo::mark();
}
c_R725::c_R725(){
}
c_R725* c_R725::m_new(){
	c_MethodInfo::m_new(String(L"Count",5),0,bb_reflection__intClass,Array<c_ClassInfo* >());
	return this;
}
void c_R725::mark(){
	c_MethodInfo::mark();
}
c_R726::c_R726(){
}
c_R726* c_R726::m_new(){
	c_MethodInfo::m_new(String(L"IsEmpty",7),0,bb_reflection__boolClass,Array<c_ClassInfo* >());
	return this;
}
void c_R726::mark(){
	c_MethodInfo::mark();
}
c_R727::c_R727(){
}
c_R727* c_R727::m_new(){
	c_ClassInfo* t_[]={bb_reflection__stringClass};
	c_MethodInfo::m_new(String(L"Contains",8),0,bb_reflection__boolClass,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R727::mark(){
	c_MethodInfo::mark();
}
c_R728::c_R728(){
}
c_R728* c_R728::m_new(){
	c_ClassInfo* t_[]={bb_reflection__stringClass,bb_reflection__stringClass};
	c_MethodInfo::m_new(String(L"Set",3),0,bb_reflection__boolClass,Array<c_ClassInfo* >(t_,2));
	return this;
}
void c_R728::mark(){
	c_MethodInfo::mark();
}
c_R729::c_R729(){
}
c_R729* c_R729::m_new(){
	c_ClassInfo* t_[]={bb_reflection__stringClass,bb_reflection__stringClass};
	c_MethodInfo::m_new(String(L"Add",3),0,bb_reflection__boolClass,Array<c_ClassInfo* >(t_,2));
	return this;
}
void c_R729::mark(){
	c_MethodInfo::mark();
}
c_R730::c_R730(){
}
c_R730* c_R730::m_new(){
	c_ClassInfo* t_[]={bb_reflection__stringClass,bb_reflection__stringClass};
	c_MethodInfo::m_new(String(L"Update",6),0,bb_reflection__boolClass,Array<c_ClassInfo* >(t_,2));
	return this;
}
void c_R730::mark(){
	c_MethodInfo::mark();
}
c_R731::c_R731(){
}
c_R731* c_R731::m_new(){
	c_ClassInfo* t_[]={bb_reflection__stringClass};
	c_MethodInfo::m_new(String(L"Get",3),0,bb_reflection__stringClass,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R731::mark(){
	c_MethodInfo::mark();
}
c_R732::c_R732(){
}
c_R732* c_R732::m_new(){
	c_ClassInfo* t_[]={bb_reflection__stringClass};
	c_MethodInfo::m_new(String(L"Remove",6),0,bb_reflection__intClass,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R732::mark(){
	c_MethodInfo::mark();
}
c_R733::c_R733(){
}
c_R733* c_R733::m_new(){
	c_MethodInfo::m_new(String(L"Keys",4),0,bb_reflection__classes[51],Array<c_ClassInfo* >());
	return this;
}
void c_R733::mark(){
	c_MethodInfo::mark();
}
c_R734::c_R734(){
}
c_R734* c_R734::m_new(){
	c_MethodInfo::m_new(String(L"Values",6),0,bb_reflection__classes[80],Array<c_ClassInfo* >());
	return this;
}
void c_R734::mark(){
	c_MethodInfo::mark();
}
c_R735::c_R735(){
}
c_R735* c_R735::m_new(){
	c_MethodInfo::m_new(String(L"ObjectEnumerator",16),0,bb_reflection__classes[81],Array<c_ClassInfo* >());
	return this;
}
void c_R735::mark(){
	c_MethodInfo::mark();
}
c_R736::c_R736(){
}
c_R736* c_R736::m_new(){
	c_ClassInfo* t_[]={bb_reflection__stringClass,bb_reflection__stringClass};
	c_MethodInfo::m_new(String(L"Insert",6),0,bb_reflection__boolClass,Array<c_ClassInfo* >(t_,2));
	return this;
}
void c_R736::mark(){
	c_MethodInfo::mark();
}
c_R737::c_R737(){
}
c_R737* c_R737::m_new(){
	c_ClassInfo* t_[]={bb_reflection__stringClass};
	c_MethodInfo::m_new(String(L"ValueForKey",11),0,bb_reflection__stringClass,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R737::mark(){
	c_MethodInfo::mark();
}
c_R738::c_R738(){
}
c_R738* c_R738::m_new(){
	c_MethodInfo::m_new(String(L"FirstNode",9),0,bb_reflection__classes[50],Array<c_ClassInfo* >());
	return this;
}
void c_R738::mark(){
	c_MethodInfo::mark();
}
c_R739::c_R739(){
}
c_R739* c_R739::m_new(){
	c_MethodInfo::m_new(String(L"LastNode",8),0,bb_reflection__classes[50],Array<c_ClassInfo* >());
	return this;
}
void c_R739::mark(){
	c_MethodInfo::mark();
}
c_R740::c_R740(){
}
c_R740* c_R740::m_new(){
	c_ClassInfo* t_[]={bb_reflection__stringClass};
	c_MethodInfo::m_new(String(L"FindNode",8),0,bb_reflection__classes[50],Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R740::mark(){
	c_MethodInfo::mark();
}
c_R741::c_R741(){
}
c_R741* c_R741::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[50]};
	c_MethodInfo::m_new(String(L"RemoveNode",10),0,bb_reflection__intClass,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R741::mark(){
	c_MethodInfo::mark();
}
c_R742::c_R742(){
}
c_R742* c_R742::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[50]};
	c_MethodInfo::m_new(String(L"InsertFixup",11),0,bb_reflection__intClass,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R742::mark(){
	c_MethodInfo::mark();
}
c_R743::c_R743(){
}
c_R743* c_R743::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[50]};
	c_MethodInfo::m_new(String(L"RotateLeft",10),0,bb_reflection__intClass,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R743::mark(){
	c_MethodInfo::mark();
}
c_R744::c_R744(){
}
c_R744* c_R744::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[50]};
	c_MethodInfo::m_new(String(L"RotateRight",11),0,bb_reflection__intClass,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R744::mark(){
	c_MethodInfo::mark();
}
c_R745::c_R745(){
}
c_R745* c_R745::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[50],bb_reflection__classes[50]};
	c_MethodInfo::m_new(String(L"DeleteFixup",11),0,bb_reflection__intClass,Array<c_ClassInfo* >(t_,2));
	return this;
}
void c_R745::mark(){
	c_MethodInfo::mark();
}
c_R747::c_R747(){
}
c_R747* c_R747::m_new(){
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[48],Array<c_ClassInfo* >());
	return this;
}
void c_R747::mark(){
	c_FunctionInfo::mark();
}
c_R749::c_R749(){
}
c_R749* c_R749::m_new(){
	c_ClassInfo* t_[]={bb_reflection__stringClass,bb_reflection__stringClass};
	c_MethodInfo::m_new(String(L"Compare",7),0,bb_reflection__intClass,Array<c_ClassInfo* >(t_,2));
	return this;
}
void c_R749::mark(){
	c_MethodInfo::mark();
}
c_R750::c_R750(){
}
c_R750* c_R750::m_new(){
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[49],Array<c_ClassInfo* >());
	return this;
}
void c_R750::mark(){
	c_FunctionInfo::mark();
}
c_R759::c_R759(){
}
c_R759* c_R759::m_new(){
	c_FieldInfo::m_new(String(L"key",3),2,bb_reflection__stringClass);
	return this;
}
void c_R759::mark(){
	c_FieldInfo::mark();
}
c_R760::c_R760(){
}
c_R760* c_R760::m_new(){
	c_FieldInfo::m_new(String(L"value",5),2,bb_reflection__stringClass);
	return this;
}
void c_R760::mark(){
	c_FieldInfo::mark();
}
c_R761::c_R761(){
}
c_R761* c_R761::m_new(){
	c_FieldInfo::m_new(String(L"color",5),2,bb_reflection__intClass);
	return this;
}
void c_R761::mark(){
	c_FieldInfo::mark();
}
c_R762::c_R762(){
}
c_R762* c_R762::m_new(){
	c_FieldInfo::m_new(String(L"parent",6),2,bb_reflection__classes[50]);
	return this;
}
void c_R762::mark(){
	c_FieldInfo::mark();
}
c_R763::c_R763(){
}
c_R763* c_R763::m_new(){
	c_FieldInfo::m_new(String(L"left",4),2,bb_reflection__classes[50]);
	return this;
}
void c_R763::mark(){
	c_FieldInfo::mark();
}
c_R764::c_R764(){
}
c_R764* c_R764::m_new(){
	c_FieldInfo::m_new(String(L"right",5),2,bb_reflection__classes[50]);
	return this;
}
void c_R764::mark(){
	c_FieldInfo::mark();
}
c_R753::c_R753(){
}
c_R753* c_R753::m_new(){
	c_ClassInfo* t_[]={bb_reflection__intClass};
	c_MethodInfo::m_new(String(L"Count",5),0,bb_reflection__intClass,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R753::mark(){
	c_MethodInfo::mark();
}
c_R754::c_R754(){
}
c_R754* c_R754::m_new(){
	c_MethodInfo::m_new(String(L"Key",3),0,bb_reflection__stringClass,Array<c_ClassInfo* >());
	return this;
}
void c_R754::mark(){
	c_MethodInfo::mark();
}
c_R755::c_R755(){
}
c_R755* c_R755::m_new(){
	c_MethodInfo::m_new(String(L"Value",5),0,bb_reflection__stringClass,Array<c_ClassInfo* >());
	return this;
}
void c_R755::mark(){
	c_MethodInfo::mark();
}
c_R756::c_R756(){
}
c_R756* c_R756::m_new(){
	c_MethodInfo::m_new(String(L"NextNode",8),0,bb_reflection__classes[50],Array<c_ClassInfo* >());
	return this;
}
void c_R756::mark(){
	c_MethodInfo::mark();
}
c_R757::c_R757(){
}
c_R757* c_R757::m_new(){
	c_MethodInfo::m_new(String(L"PrevNode",8),0,bb_reflection__classes[50],Array<c_ClassInfo* >());
	return this;
}
void c_R757::mark(){
	c_MethodInfo::mark();
}
c_R758::c_R758(){
}
c_R758* c_R758::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[50]};
	c_MethodInfo::m_new(String(L"Copy",4),0,bb_reflection__classes[50],Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R758::mark(){
	c_MethodInfo::mark();
}
c_R752::c_R752(){
}
c_R752* c_R752::m_new(){
	c_ClassInfo* t_[]={bb_reflection__stringClass,bb_reflection__stringClass,bb_reflection__intClass,bb_reflection__classes[50]};
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[50],Array<c_ClassInfo* >(t_,4));
	return this;
}
void c_R752::mark(){
	c_FunctionInfo::mark();
}
c_R765::c_R765(){
}
c_R765* c_R765::m_new(){
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[50],Array<c_ClassInfo* >());
	return this;
}
void c_R765::mark(){
	c_FunctionInfo::mark();
}
c_R769::c_R769(){
}
c_R769* c_R769::m_new(){
	c_FieldInfo::m_new(String(L"map",3),2,bb_reflection__classes[48]);
	return this;
}
void c_R769::mark(){
	c_FieldInfo::mark();
}
c_R768::c_R768(){
}
c_R768* c_R768::m_new(){
	c_MethodInfo::m_new(String(L"ObjectEnumerator",16),0,bb_reflection__classes[52],Array<c_ClassInfo* >());
	return this;
}
void c_R768::mark(){
	c_MethodInfo::mark();
}
c_R767::c_R767(){
}
c_R767* c_R767::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[48]};
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[51],Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R767::mark(){
	c_FunctionInfo::mark();
}
c_R770::c_R770(){
}
c_R770* c_R770::m_new(){
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[51],Array<c_ClassInfo* >());
	return this;
}
void c_R770::mark(){
	c_FunctionInfo::mark();
}
c_R775::c_R775(){
}
c_R775* c_R775::m_new(){
	c_FieldInfo::m_new(String(L"node",4),2,bb_reflection__classes[50]);
	return this;
}
void c_R775::mark(){
	c_FieldInfo::mark();
}
c_R773::c_R773(){
}
c_R773* c_R773::m_new(){
	c_MethodInfo::m_new(String(L"HasNext",7),0,bb_reflection__boolClass,Array<c_ClassInfo* >());
	return this;
}
void c_R773::mark(){
	c_MethodInfo::mark();
}
c_R774::c_R774(){
}
c_R774* c_R774::m_new(){
	c_MethodInfo::m_new(String(L"NextObject",10),0,bb_reflection__stringClass,Array<c_ClassInfo* >());
	return this;
}
void c_R774::mark(){
	c_MethodInfo::mark();
}
c_R772::c_R772(){
}
c_R772* c_R772::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[50]};
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[52],Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R772::mark(){
	c_FunctionInfo::mark();
}
c_R776::c_R776(){
}
c_R776* c_R776::m_new(){
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[52],Array<c_ClassInfo* >());
	return this;
}
void c_R776::mark(){
	c_FunctionInfo::mark();
}
c_R778::c_R778(){
}
c_R778* c_R778::m_new(){
	c_FieldInfo::m_new(String(L"color",5),0,bb_reflection__classes[44]);
	return this;
}
void c_R778::mark(){
	c_FieldInfo::mark();
}
c_R779::c_R779(){
}
c_R779* c_R779::m_new(){
	c_FieldInfo::m_new(String(L"renderOutline",13),0,bb_reflection__boolClass);
	return this;
}
void c_R779::mark(){
	c_FieldInfo::mark();
}
c_R780::c_R780(){
}
c_R780* c_R780::m_new(){
	c_MethodInfo::m_new(String(L"Radius",6),4,bb_reflection__floatClass,Array<c_ClassInfo* >());
	return this;
}
void c_R780::mark(){
	c_MethodInfo::mark();
}
c_R781::c_R781(){
}
c_R781* c_R781::m_new(){
	c_MethodInfo::m_new(String(L"Render",6),0,0,Array<c_ClassInfo* >());
	return this;
}
void c_R781::mark(){
	c_MethodInfo::mark();
}
c_R782::c_R782(){
}
c_R782* c_R782::m_new(){
	c_MethodInfo::m_new(String(L"Draw",4),4,0,Array<c_ClassInfo* >());
	return this;
}
void c_R782::mark(){
	c_MethodInfo::mark();
}
c_R783::c_R783(){
}
c_R783* c_R783::m_new(){
	c_MethodInfo::m_new(String(L"DrawOutline",11),4,0,Array<c_ClassInfo* >());
	return this;
}
void c_R783::mark(){
	c_MethodInfo::mark();
}
c_R784::c_R784(){
}
c_R784* c_R784::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[46]};
	c_MethodInfo::m_new(String(L"PointInside",11),4,bb_reflection__boolClass,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R784::mark(){
	c_MethodInfo::mark();
}
c_R785::c_R785(){
}
c_R785* c_R785::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[53]};
	c_MethodInfo::m_new(String(L"CollidesWith",12),0,bb_reflection__boolClass,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R785::mark(){
	c_MethodInfo::mark();
}
c_R786::c_R786(){
}
c_R786* c_R786::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[54]};
	c_MethodInfo::m_new(String(L"CollidesWith",12),4,bb_reflection__boolClass,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R786::mark(){
	c_MethodInfo::mark();
}
c_R787::c_R787(){
}
c_R787* c_R787::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[55]};
	c_MethodInfo::m_new(String(L"CollidesWith",12),4,bb_reflection__boolClass,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R787::mark(){
	c_MethodInfo::mark();
}
c_R788::c_R788(){
}
c_R788* c_R788::m_new(){
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[53],Array<c_ClassInfo* >());
	return this;
}
void c_R788::mark(){
	c_FunctionInfo::mark();
}
c_R790::c_R790(){
}
c_R790* c_R790::m_new(){
	c_FieldInfo::m_new(String(L"size",4),0,bb_reflection__classes[46]);
	return this;
}
void c_R790::mark(){
	c_FieldInfo::mark();
}
c_R793::c_R793(){
}
c_R793* c_R793::m_new(){
	c_MethodInfo::m_new(String(L"Copy",4),0,bb_reflection__classes[54],Array<c_ClassInfo* >());
	return this;
}
void c_R793::mark(){
	c_MethodInfo::mark();
}
c_R794::c_R794(){
}
c_R794* c_R794::m_new(){
	c_MethodInfo::m_new(String(L"Radius",6),0,bb_reflection__floatClass,Array<c_ClassInfo* >());
	return this;
}
void c_R794::mark(){
	c_MethodInfo::mark();
}
c_R795::c_R795(){
}
c_R795* c_R795::m_new(){
	c_MethodInfo::m_new(String(L"Draw",4),0,0,Array<c_ClassInfo* >());
	return this;
}
void c_R795::mark(){
	c_MethodInfo::mark();
}
c_R796::c_R796(){
}
c_R796* c_R796::m_new(){
	c_MethodInfo::m_new(String(L"DrawOutline",11),0,0,Array<c_ClassInfo* >());
	return this;
}
void c_R796::mark(){
	c_MethodInfo::mark();
}
c_R797::c_R797(){
}
c_R797* c_R797::m_new(){
	c_MethodInfo::m_new(String(L"TopLeft",7),0,bb_reflection__classes[46],Array<c_ClassInfo* >());
	return this;
}
void c_R797::mark(){
	c_MethodInfo::mark();
}
c_R798::c_R798(){
}
c_R798* c_R798::m_new(){
	c_MethodInfo::m_new(String(L"TopRight",8),0,bb_reflection__classes[46],Array<c_ClassInfo* >());
	return this;
}
void c_R798::mark(){
	c_MethodInfo::mark();
}
c_R799::c_R799(){
}
c_R799* c_R799::m_new(){
	c_MethodInfo::m_new(String(L"BottomLeft",10),0,bb_reflection__classes[46],Array<c_ClassInfo* >());
	return this;
}
void c_R799::mark(){
	c_MethodInfo::mark();
}
c_R800::c_R800(){
}
c_R800* c_R800::m_new(){
	c_MethodInfo::m_new(String(L"BottomRight",11),0,bb_reflection__classes[46],Array<c_ClassInfo* >());
	return this;
}
void c_R800::mark(){
	c_MethodInfo::mark();
}
c_R801::c_R801(){
}
c_R801* c_R801::m_new(){
	c_MethodInfo::m_new(String(L"TopLeftUntransformed",20),0,bb_reflection__classes[46],Array<c_ClassInfo* >());
	return this;
}
void c_R801::mark(){
	c_MethodInfo::mark();
}
c_R802::c_R802(){
}
c_R802* c_R802::m_new(){
	c_MethodInfo::m_new(String(L"TopRightUntransformed",21),0,bb_reflection__classes[46],Array<c_ClassInfo* >());
	return this;
}
void c_R802::mark(){
	c_MethodInfo::mark();
}
c_R803::c_R803(){
}
c_R803* c_R803::m_new(){
	c_MethodInfo::m_new(String(L"BottomLeftUntransformed",23),0,bb_reflection__classes[46],Array<c_ClassInfo* >());
	return this;
}
void c_R803::mark(){
	c_MethodInfo::mark();
}
c_R804::c_R804(){
}
c_R804* c_R804::m_new(){
	c_MethodInfo::m_new(String(L"BottomRightUntransformed",24),0,bb_reflection__classes[46],Array<c_ClassInfo* >());
	return this;
}
void c_R804::mark(){
	c_MethodInfo::mark();
}
c_R805::c_R805(){
}
c_R805* c_R805::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[46]};
	c_MethodInfo::m_new(String(L"PointInside",11),0,bb_reflection__boolClass,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R805::mark(){
	c_MethodInfo::mark();
}
c_R806::c_R806(){
}
c_R806* c_R806::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[54]};
	c_MethodInfo::m_new(String(L"CollidesWith",12),0,bb_reflection__boolClass,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R806::mark(){
	c_MethodInfo::mark();
}
c_R807::c_R807(){
}
c_R807* c_R807::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[55]};
	c_MethodInfo::m_new(String(L"CollidesWith",12),0,bb_reflection__boolClass,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R807::mark(){
	c_MethodInfo::mark();
}
c_R791::c_R791(){
}
c_R791* c_R791::m_new(){
	c_ClassInfo* t_[]={bb_reflection__floatClass,bb_reflection__floatClass,bb_reflection__floatClass,bb_reflection__floatClass};
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[54],Array<c_ClassInfo* >(t_,4));
	return this;
}
void c_R791::mark(){
	c_FunctionInfo::mark();
}
c_R792::c_R792(){
}
c_R792* c_R792::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[46],bb_reflection__classes[46],bb_reflection__classes[46],bb_reflection__classes[46]};
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[54],Array<c_ClassInfo* >(t_,4));
	return this;
}
void c_R792::mark(){
	c_FunctionInfo::mark();
}
c_R808::c_R808(){
}
c_R808* c_R808::m_new(){
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[54],Array<c_ClassInfo* >());
	return this;
}
void c_R808::mark(){
	c_FunctionInfo::mark();
}
c_R810::c_R810(){
}
c_R810* c_R810::m_new(){
	c_FieldInfo::m_new(String(L"radius",6),0,bb_reflection__floatClass);
	return this;
}
void c_R810::mark(){
	c_FieldInfo::mark();
}
c_R812::c_R812(){
}
c_R812* c_R812::m_new(){
	c_MethodInfo::m_new(String(L"Copy",4),0,bb_reflection__classes[55],Array<c_ClassInfo* >());
	return this;
}
void c_R812::mark(){
	c_MethodInfo::mark();
}
c_R813::c_R813(){
}
c_R813* c_R813::m_new(){
	c_MethodInfo::m_new(String(L"Draw",4),0,0,Array<c_ClassInfo* >());
	return this;
}
void c_R813::mark(){
	c_MethodInfo::mark();
}
c_R814::c_R814(){
}
c_R814* c_R814::m_new(){
	c_MethodInfo::m_new(String(L"DrawOutline",11),0,0,Array<c_ClassInfo* >());
	return this;
}
void c_R814::mark(){
	c_MethodInfo::mark();
}
c_R815::c_R815(){
}
c_R815* c_R815::m_new(){
	c_MethodInfo::m_new(String(L"Radius",6),0,bb_reflection__floatClass,Array<c_ClassInfo* >());
	return this;
}
void c_R815::mark(){
	c_MethodInfo::mark();
}
c_R816::c_R816(){
}
c_R816* c_R816::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[46]};
	c_MethodInfo::m_new(String(L"PointInside",11),0,bb_reflection__boolClass,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R816::mark(){
	c_MethodInfo::mark();
}
c_R817::c_R817(){
}
c_R817* c_R817::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[55]};
	c_MethodInfo::m_new(String(L"CollidesWith",12),0,bb_reflection__boolClass,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R817::mark(){
	c_MethodInfo::mark();
}
c_R818::c_R818(){
}
c_R818* c_R818::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[54]};
	c_MethodInfo::m_new(String(L"CollidesWith",12),0,bb_reflection__boolClass,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R818::mark(){
	c_MethodInfo::mark();
}
c_R819::c_R819(){
}
c_R819* c_R819::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[46],bb_reflection__classes[46]};
	c_MethodInfo::m_new(String(L"CollidesWithLine",16),0,bb_reflection__boolClass,Array<c_ClassInfo* >(t_,2));
	return this;
}
void c_R819::mark(){
	c_MethodInfo::mark();
}
c_R811::c_R811(){
}
c_R811* c_R811::m_new(){
	c_ClassInfo* t_[]={bb_reflection__floatClass,bb_reflection__floatClass,bb_reflection__floatClass};
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[55],Array<c_ClassInfo* >(t_,3));
	return this;
}
void c_R811::mark(){
	c_FunctionInfo::mark();
}
c_R820::c_R820(){
}
c_R820* c_R820::m_new(){
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[55],Array<c_ClassInfo* >());
	return this;
}
void c_R820::mark(){
	c_FunctionInfo::mark();
}
c_R822::c_R822(){
}
c_R822* c_R822::m_new(){
	c_FieldInfo::m_new(String(L"color",5),0,bb_reflection__classes[44]);
	return this;
}
void c_R822::mark(){
	c_FieldInfo::mark();
}
c_R823::c_R823(){
}
c_R823* c_R823::m_new(){
	c_FieldInfo::m_new(String(L"hidden",6),0,bb_reflection__boolClass);
	return this;
}
void c_R823::mark(){
	c_FieldInfo::mark();
}
c_R824::c_R824(){
}
c_R824* c_R824::m_new(){
	c_FieldInfo::m_new(String(L"flipX",5),0,bb_reflection__boolClass);
	return this;
}
void c_R824::mark(){
	c_FieldInfo::mark();
}
c_R825::c_R825(){
}
c_R825* c_R825::m_new(){
	c_FieldInfo::m_new(String(L"flipY",5),0,bb_reflection__boolClass);
	return this;
}
void c_R825::mark(){
	c_FieldInfo::mark();
}
c_R841::c_R841(){
}
c_R841* c_R841::m_new(){
	c_FieldInfo::m_new(String(L"imagePath",9),2,bb_reflection__stringClass);
	return this;
}
void c_R841::mark(){
	c_FieldInfo::mark();
}
c_R827::c_R827(){
}
c_R827* c_R827::m_new(){
	c_MethodInfo::m_new(String(L"Copy",4),0,bb_reflection__classes[56],Array<c_ClassInfo* >());
	return this;
}
void c_R827::mark(){
	c_MethodInfo::mark();
}
c_R828::c_R828(){
}
c_R828* c_R828::m_new(){
	c_ClassInfo* t_[]={bb_reflection__stringClass,bb_reflection__intClass};
	c_MethodInfo::m_new(String(L"SetImage",8),0,0,Array<c_ClassInfo* >(t_,2));
	return this;
}
void c_R828::mark(){
	c_MethodInfo::mark();
}
c_R829::c_R829(){
}
c_R829* c_R829::m_new(){
	c_ClassInfo* t_[]={bb_reflection__floatClass,bb_reflection__floatClass};
	c_MethodInfo::m_new(String(L"SetHandle",9),0,0,Array<c_ClassInfo* >(t_,2));
	return this;
}
void c_R829::mark(){
	c_MethodInfo::mark();
}
c_R830::c_R830(){
}
c_R830* c_R830::m_new(){
	c_ClassInfo* t_[]={bb_reflection__floatClass,bb_reflection__floatClass,bb_reflection__floatClass};
	c_MethodInfo::m_new(String(L"SetColor",8),0,0,Array<c_ClassInfo* >(t_,3));
	return this;
}
void c_R830::mark(){
	c_MethodInfo::mark();
}
c_R831::c_R831(){
}
c_R831* c_R831::m_new(){
	c_MethodInfo::m_new(String(L"Width",5),0,bb_reflection__floatClass,Array<c_ClassInfo* >());
	return this;
}
void c_R831::mark(){
	c_MethodInfo::mark();
}
c_R832::c_R832(){
}
c_R832* c_R832::m_new(){
	c_MethodInfo::m_new(String(L"Height",6),0,bb_reflection__floatClass,Array<c_ClassInfo* >());
	return this;
}
void c_R832::mark(){
	c_MethodInfo::mark();
}
c_R833::c_R833(){
}
c_R833* c_R833::m_new(){
	c_MethodInfo::m_new(String(L"ImagePath",9),0,bb_reflection__stringClass,Array<c_ClassInfo* >());
	return this;
}
void c_R833::mark(){
	c_MethodInfo::mark();
}
c_R834::c_R834(){
}
c_R834* c_R834::m_new(){
	c_MethodInfo::m_new(String(L"HandleX",7),0,bb_reflection__floatClass,Array<c_ClassInfo* >());
	return this;
}
void c_R834::mark(){
	c_MethodInfo::mark();
}
c_R835::c_R835(){
}
c_R835* c_R835::m_new(){
	c_MethodInfo::m_new(String(L"HandleY",7),0,bb_reflection__floatClass,Array<c_ClassInfo* >());
	return this;
}
void c_R835::mark(){
	c_MethodInfo::mark();
}
c_R836::c_R836(){
}
c_R836* c_R836::m_new(){
	c_ClassInfo* t_[]={bb_reflection__floatClass};
	c_MethodInfo::m_new(String(L"Alpha",5),0,0,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R836::mark(){
	c_MethodInfo::mark();
}
c_R837::c_R837(){
}
c_R837* c_R837::m_new(){
	c_MethodInfo::m_new(String(L"Alpha",5),0,bb_reflection__floatClass,Array<c_ClassInfo* >());
	return this;
}
void c_R837::mark(){
	c_MethodInfo::mark();
}
c_R838::c_R838(){
}
c_R838* c_R838::m_new(){
	c_MethodInfo::m_new(String(L"Render",6),0,0,Array<c_ClassInfo* >());
	return this;
}
void c_R838::mark(){
	c_MethodInfo::mark();
}
c_R839::c_R839(){
}
c_R839* c_R839::m_new(){
	c_MethodInfo::m_new(String(L"DrawImage",9),0,0,Array<c_ClassInfo* >());
	return this;
}
void c_R839::mark(){
	c_MethodInfo::mark();
}
c_R840::c_R840(){
}
c_R840* c_R840::m_new(){
	c_ClassInfo* t_[]={bb_reflection__floatClass};
	c_MethodInfo::m_new(String(L"Update",6),0,0,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R840::mark(){
	c_MethodInfo::mark();
}
c_R826::c_R826(){
}
c_R826* c_R826::m_new(){
	c_ClassInfo* t_[]={bb_reflection__stringClass,bb_reflection__floatClass,bb_reflection__floatClass};
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[56],Array<c_ClassInfo* >(t_,3));
	return this;
}
void c_R826::mark(){
	c_FunctionInfo::mark();
}
c_R842::c_R842(){
}
c_R842* c_R842::m_new(){
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[56],Array<c_ClassInfo* >());
	return this;
}
void c_R842::mark(){
	c_FunctionInfo::mark();
}
c_R847::c_R847(){
}
c_R847* c_R847::m_new(){
	c_FieldInfo::m_new(String(L"_deque",6),2,bb_reflection__classes[5]);
	return this;
}
void c_R847::mark(){
	c_FieldInfo::mark();
}
c_R848::c_R848(){
}
c_R848* c_R848::m_new(){
	c_FieldInfo::m_new(String(L"_index",6),2,bb_reflection__intClass);
	return this;
}
void c_R848::mark(){
	c_FieldInfo::mark();
}
c_R845::c_R845(){
}
c_R845* c_R845::m_new(){
	c_MethodInfo::m_new(String(L"HasNext",7),0,bb_reflection__boolClass,Array<c_ClassInfo* >());
	return this;
}
void c_R845::mark(){
	c_MethodInfo::mark();
}
c_R846::c_R846(){
}
c_R846* c_R846::m_new(){
	c_MethodInfo::m_new(String(L"NextObject",10),0,bb_reflection__intClass,Array<c_ClassInfo* >());
	return this;
}
void c_R846::mark(){
	c_MethodInfo::mark();
}
c_R844::c_R844(){
}
c_R844* c_R844::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[5]};
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[57],Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R844::mark(){
	c_FunctionInfo::mark();
}
c_R849::c_R849(){
}
c_R849* c_R849::m_new(){
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[57],Array<c_ClassInfo* >());
	return this;
}
void c_R849::mark(){
	c_FunctionInfo::mark();
}
c_R854::c_R854(){
}
c_R854* c_R854::m_new(){
	c_FieldInfo::m_new(String(L"_deque",6),2,bb_reflection__classes[7]);
	return this;
}
void c_R854::mark(){
	c_FieldInfo::mark();
}
c_R855::c_R855(){
}
c_R855* c_R855::m_new(){
	c_FieldInfo::m_new(String(L"_index",6),2,bb_reflection__intClass);
	return this;
}
void c_R855::mark(){
	c_FieldInfo::mark();
}
c_R852::c_R852(){
}
c_R852* c_R852::m_new(){
	c_MethodInfo::m_new(String(L"HasNext",7),0,bb_reflection__boolClass,Array<c_ClassInfo* >());
	return this;
}
void c_R852::mark(){
	c_MethodInfo::mark();
}
c_R853::c_R853(){
}
c_R853* c_R853::m_new(){
	c_MethodInfo::m_new(String(L"NextObject",10),0,bb_reflection__floatClass,Array<c_ClassInfo* >());
	return this;
}
void c_R853::mark(){
	c_MethodInfo::mark();
}
c_R851::c_R851(){
}
c_R851* c_R851::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[7]};
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[58],Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R851::mark(){
	c_FunctionInfo::mark();
}
c_R856::c_R856(){
}
c_R856* c_R856::m_new(){
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[58],Array<c_ClassInfo* >());
	return this;
}
void c_R856::mark(){
	c_FunctionInfo::mark();
}
c_R861::c_R861(){
}
c_R861* c_R861::m_new(){
	c_FieldInfo::m_new(String(L"_deque",6),2,bb_reflection__classes[9]);
	return this;
}
void c_R861::mark(){
	c_FieldInfo::mark();
}
c_R862::c_R862(){
}
c_R862* c_R862::m_new(){
	c_FieldInfo::m_new(String(L"_index",6),2,bb_reflection__intClass);
	return this;
}
void c_R862::mark(){
	c_FieldInfo::mark();
}
c_R859::c_R859(){
}
c_R859* c_R859::m_new(){
	c_MethodInfo::m_new(String(L"HasNext",7),0,bb_reflection__boolClass,Array<c_ClassInfo* >());
	return this;
}
void c_R859::mark(){
	c_MethodInfo::mark();
}
c_R860::c_R860(){
}
c_R860* c_R860::m_new(){
	c_MethodInfo::m_new(String(L"NextObject",10),0,bb_reflection__stringClass,Array<c_ClassInfo* >());
	return this;
}
void c_R860::mark(){
	c_MethodInfo::mark();
}
c_R858::c_R858(){
}
c_R858* c_R858::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[9]};
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[59],Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R858::mark(){
	c_FunctionInfo::mark();
}
c_R863::c_R863(){
}
c_R863* c_R863::m_new(){
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[59],Array<c_ClassInfo* >());
	return this;
}
void c_R863::mark(){
	c_FunctionInfo::mark();
}
c_R868::c_R868(){
}
c_R868* c_R868::m_new(){
	c_FieldInfo::m_new(String(L"_list",5),2,bb_reflection__classes[12]);
	return this;
}
void c_R868::mark(){
	c_FieldInfo::mark();
}
c_R869::c_R869(){
}
c_R869* c_R869::m_new(){
	c_FieldInfo::m_new(String(L"_curr",5),2,bb_reflection__classes[14]);
	return this;
}
void c_R869::mark(){
	c_FieldInfo::mark();
}
c_R866::c_R866(){
}
c_R866* c_R866::m_new(){
	c_MethodInfo::m_new(String(L"HasNext",7),0,bb_reflection__boolClass,Array<c_ClassInfo* >());
	return this;
}
void c_R866::mark(){
	c_MethodInfo::mark();
}
c_R867::c_R867(){
}
c_R867* c_R867::m_new(){
	c_MethodInfo::m_new(String(L"NextObject",10),0,bb_reflection__intClass,Array<c_ClassInfo* >());
	return this;
}
void c_R867::mark(){
	c_MethodInfo::mark();
}
c_R865::c_R865(){
}
c_R865* c_R865::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[12]};
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[60],Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R865::mark(){
	c_FunctionInfo::mark();
}
c_R870::c_R870(){
}
c_R870* c_R870::m_new(){
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[60],Array<c_ClassInfo* >());
	return this;
}
void c_R870::mark(){
	c_FunctionInfo::mark();
}
c_R874::c_R874(){
}
c_R874* c_R874::m_new(){
	c_FieldInfo::m_new(String(L"_list",5),2,bb_reflection__classes[12]);
	return this;
}
void c_R874::mark(){
	c_FieldInfo::mark();
}
c_R873::c_R873(){
}
c_R873* c_R873::m_new(){
	c_MethodInfo::m_new(String(L"ObjectEnumerator",16),0,bb_reflection__classes[65],Array<c_ClassInfo* >());
	return this;
}
void c_R873::mark(){
	c_MethodInfo::mark();
}
c_R872::c_R872(){
}
c_R872* c_R872::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[12]};
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[61],Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R872::mark(){
	c_FunctionInfo::mark();
}
c_R875::c_R875(){
}
c_R875* c_R875::m_new(){
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[61],Array<c_ClassInfo* >());
	return this;
}
void c_R875::mark(){
	c_FunctionInfo::mark();
}
c_R880::c_R880(){
}
c_R880* c_R880::m_new(){
	c_FieldInfo::m_new(String(L"_list",5),2,bb_reflection__classes[16]);
	return this;
}
void c_R880::mark(){
	c_FieldInfo::mark();
}
c_R881::c_R881(){
}
c_R881* c_R881::m_new(){
	c_FieldInfo::m_new(String(L"_curr",5),2,bb_reflection__classes[18]);
	return this;
}
void c_R881::mark(){
	c_FieldInfo::mark();
}
c_R878::c_R878(){
}
c_R878* c_R878::m_new(){
	c_MethodInfo::m_new(String(L"HasNext",7),0,bb_reflection__boolClass,Array<c_ClassInfo* >());
	return this;
}
void c_R878::mark(){
	c_MethodInfo::mark();
}
c_R879::c_R879(){
}
c_R879* c_R879::m_new(){
	c_MethodInfo::m_new(String(L"NextObject",10),0,bb_reflection__floatClass,Array<c_ClassInfo* >());
	return this;
}
void c_R879::mark(){
	c_MethodInfo::mark();
}
c_R877::c_R877(){
}
c_R877* c_R877::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[16]};
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[62],Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R877::mark(){
	c_FunctionInfo::mark();
}
c_R882::c_R882(){
}
c_R882* c_R882::m_new(){
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[62],Array<c_ClassInfo* >());
	return this;
}
void c_R882::mark(){
	c_FunctionInfo::mark();
}
c_R886::c_R886(){
}
c_R886* c_R886::m_new(){
	c_FieldInfo::m_new(String(L"_list",5),2,bb_reflection__classes[16]);
	return this;
}
void c_R886::mark(){
	c_FieldInfo::mark();
}
c_R885::c_R885(){
}
c_R885* c_R885::m_new(){
	c_MethodInfo::m_new(String(L"ObjectEnumerator",16),0,bb_reflection__classes[66],Array<c_ClassInfo* >());
	return this;
}
void c_R885::mark(){
	c_MethodInfo::mark();
}
c_R884::c_R884(){
}
c_R884* c_R884::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[16]};
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[63],Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R884::mark(){
	c_FunctionInfo::mark();
}
c_R887::c_R887(){
}
c_R887* c_R887::m_new(){
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[63],Array<c_ClassInfo* >());
	return this;
}
void c_R887::mark(){
	c_FunctionInfo::mark();
}
c_R891::c_R891(){
}
c_R891* c_R891::m_new(){
	c_FieldInfo::m_new(String(L"_list",5),2,bb_reflection__classes[20]);
	return this;
}
void c_R891::mark(){
	c_FieldInfo::mark();
}
c_R890::c_R890(){
}
c_R890* c_R890::m_new(){
	c_MethodInfo::m_new(String(L"ObjectEnumerator",16),0,bb_reflection__classes[67],Array<c_ClassInfo* >());
	return this;
}
void c_R890::mark(){
	c_MethodInfo::mark();
}
c_R889::c_R889(){
}
c_R889* c_R889::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[20]};
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[64],Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R889::mark(){
	c_FunctionInfo::mark();
}
c_R892::c_R892(){
}
c_R892* c_R892::m_new(){
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[64],Array<c_ClassInfo* >());
	return this;
}
void c_R892::mark(){
	c_FunctionInfo::mark();
}
c_R897::c_R897(){
}
c_R897* c_R897::m_new(){
	c_FieldInfo::m_new(String(L"_list",5),2,bb_reflection__classes[12]);
	return this;
}
void c_R897::mark(){
	c_FieldInfo::mark();
}
c_R898::c_R898(){
}
c_R898* c_R898::m_new(){
	c_FieldInfo::m_new(String(L"_curr",5),2,bb_reflection__classes[14]);
	return this;
}
void c_R898::mark(){
	c_FieldInfo::mark();
}
c_R895::c_R895(){
}
c_R895* c_R895::m_new(){
	c_MethodInfo::m_new(String(L"HasNext",7),0,bb_reflection__boolClass,Array<c_ClassInfo* >());
	return this;
}
void c_R895::mark(){
	c_MethodInfo::mark();
}
c_R896::c_R896(){
}
c_R896* c_R896::m_new(){
	c_MethodInfo::m_new(String(L"NextObject",10),0,bb_reflection__intClass,Array<c_ClassInfo* >());
	return this;
}
void c_R896::mark(){
	c_MethodInfo::mark();
}
c_R894::c_R894(){
}
c_R894* c_R894::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[12]};
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[65],Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R894::mark(){
	c_FunctionInfo::mark();
}
c_R899::c_R899(){
}
c_R899* c_R899::m_new(){
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[65],Array<c_ClassInfo* >());
	return this;
}
void c_R899::mark(){
	c_FunctionInfo::mark();
}
c_R904::c_R904(){
}
c_R904* c_R904::m_new(){
	c_FieldInfo::m_new(String(L"_list",5),2,bb_reflection__classes[16]);
	return this;
}
void c_R904::mark(){
	c_FieldInfo::mark();
}
c_R905::c_R905(){
}
c_R905* c_R905::m_new(){
	c_FieldInfo::m_new(String(L"_curr",5),2,bb_reflection__classes[18]);
	return this;
}
void c_R905::mark(){
	c_FieldInfo::mark();
}
c_R902::c_R902(){
}
c_R902* c_R902::m_new(){
	c_MethodInfo::m_new(String(L"HasNext",7),0,bb_reflection__boolClass,Array<c_ClassInfo* >());
	return this;
}
void c_R902::mark(){
	c_MethodInfo::mark();
}
c_R903::c_R903(){
}
c_R903* c_R903::m_new(){
	c_MethodInfo::m_new(String(L"NextObject",10),0,bb_reflection__floatClass,Array<c_ClassInfo* >());
	return this;
}
void c_R903::mark(){
	c_MethodInfo::mark();
}
c_R901::c_R901(){
}
c_R901* c_R901::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[16]};
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[66],Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R901::mark(){
	c_FunctionInfo::mark();
}
c_R906::c_R906(){
}
c_R906* c_R906::m_new(){
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[66],Array<c_ClassInfo* >());
	return this;
}
void c_R906::mark(){
	c_FunctionInfo::mark();
}
c_R911::c_R911(){
}
c_R911* c_R911::m_new(){
	c_FieldInfo::m_new(String(L"_list",5),2,bb_reflection__classes[20]);
	return this;
}
void c_R911::mark(){
	c_FieldInfo::mark();
}
c_R912::c_R912(){
}
c_R912* c_R912::m_new(){
	c_FieldInfo::m_new(String(L"_curr",5),2,bb_reflection__classes[22]);
	return this;
}
void c_R912::mark(){
	c_FieldInfo::mark();
}
c_R909::c_R909(){
}
c_R909* c_R909::m_new(){
	c_MethodInfo::m_new(String(L"HasNext",7),0,bb_reflection__boolClass,Array<c_ClassInfo* >());
	return this;
}
void c_R909::mark(){
	c_MethodInfo::mark();
}
c_R910::c_R910(){
}
c_R910* c_R910::m_new(){
	c_MethodInfo::m_new(String(L"NextObject",10),0,bb_reflection__stringClass,Array<c_ClassInfo* >());
	return this;
}
void c_R910::mark(){
	c_MethodInfo::mark();
}
c_R908::c_R908(){
}
c_R908* c_R908::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[20]};
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[67],Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R908::mark(){
	c_FunctionInfo::mark();
}
c_R913::c_R913(){
}
c_R913* c_R913::m_new(){
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[67],Array<c_ClassInfo* >());
	return this;
}
void c_R913::mark(){
	c_FunctionInfo::mark();
}
c_R922::c_R922(){
}
c_R922* c_R922::m_new(){
	c_FieldInfo::m_new(String(L"key",3),2,bb_reflection__intClass);
	return this;
}
void c_R922::mark(){
	c_FieldInfo::mark();
}
c_R923::c_R923(){
}
c_R923* c_R923::m_new(){
	c_FieldInfo::m_new(String(L"value",5),2,bb_reflection__classes[0]);
	return this;
}
void c_R923::mark(){
	c_FieldInfo::mark();
}
c_R924::c_R924(){
}
c_R924* c_R924::m_new(){
	c_FieldInfo::m_new(String(L"color",5),2,bb_reflection__intClass);
	return this;
}
void c_R924::mark(){
	c_FieldInfo::mark();
}
c_R925::c_R925(){
}
c_R925* c_R925::m_new(){
	c_FieldInfo::m_new(String(L"parent",6),2,bb_reflection__classes[68]);
	return this;
}
void c_R925::mark(){
	c_FieldInfo::mark();
}
c_R926::c_R926(){
}
c_R926* c_R926::m_new(){
	c_FieldInfo::m_new(String(L"left",4),2,bb_reflection__classes[68]);
	return this;
}
void c_R926::mark(){
	c_FieldInfo::mark();
}
c_R927::c_R927(){
}
c_R927* c_R927::m_new(){
	c_FieldInfo::m_new(String(L"right",5),2,bb_reflection__classes[68]);
	return this;
}
void c_R927::mark(){
	c_FieldInfo::mark();
}
c_R916::c_R916(){
}
c_R916* c_R916::m_new(){
	c_ClassInfo* t_[]={bb_reflection__intClass};
	c_MethodInfo::m_new(String(L"Count",5),0,bb_reflection__intClass,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R916::mark(){
	c_MethodInfo::mark();
}
c_R917::c_R917(){
}
c_R917* c_R917::m_new(){
	c_MethodInfo::m_new(String(L"Key",3),0,bb_reflection__intClass,Array<c_ClassInfo* >());
	return this;
}
void c_R917::mark(){
	c_MethodInfo::mark();
}
c_R918::c_R918(){
}
c_R918* c_R918::m_new(){
	c_MethodInfo::m_new(String(L"Value",5),0,bb_reflection__classes[0],Array<c_ClassInfo* >());
	return this;
}
void c_R918::mark(){
	c_MethodInfo::mark();
}
c_R919::c_R919(){
}
c_R919* c_R919::m_new(){
	c_MethodInfo::m_new(String(L"NextNode",8),0,bb_reflection__classes[68],Array<c_ClassInfo* >());
	return this;
}
void c_R919::mark(){
	c_MethodInfo::mark();
}
c_R920::c_R920(){
}
c_R920* c_R920::m_new(){
	c_MethodInfo::m_new(String(L"PrevNode",8),0,bb_reflection__classes[68],Array<c_ClassInfo* >());
	return this;
}
void c_R920::mark(){
	c_MethodInfo::mark();
}
c_R921::c_R921(){
}
c_R921* c_R921::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[68]};
	c_MethodInfo::m_new(String(L"Copy",4),0,bb_reflection__classes[68],Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R921::mark(){
	c_MethodInfo::mark();
}
c_R915::c_R915(){
}
c_R915* c_R915::m_new(){
	c_ClassInfo* t_[]={bb_reflection__intClass,bb_reflection__classes[0],bb_reflection__intClass,bb_reflection__classes[68]};
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[68],Array<c_ClassInfo* >(t_,4));
	return this;
}
void c_R915::mark(){
	c_FunctionInfo::mark();
}
c_R928::c_R928(){
}
c_R928* c_R928::m_new(){
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[68],Array<c_ClassInfo* >());
	return this;
}
void c_R928::mark(){
	c_FunctionInfo::mark();
}
c_R932::c_R932(){
}
c_R932* c_R932::m_new(){
	c_FieldInfo::m_new(String(L"map",3),2,bb_reflection__classes[27]);
	return this;
}
void c_R932::mark(){
	c_FieldInfo::mark();
}
c_R931::c_R931(){
}
c_R931* c_R931::m_new(){
	c_MethodInfo::m_new(String(L"ObjectEnumerator",16),0,bb_reflection__classes[82],Array<c_ClassInfo* >());
	return this;
}
void c_R931::mark(){
	c_MethodInfo::mark();
}
c_R930::c_R930(){
}
c_R930* c_R930::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[27]};
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[69],Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R930::mark(){
	c_FunctionInfo::mark();
}
c_R933::c_R933(){
}
c_R933* c_R933::m_new(){
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[69],Array<c_ClassInfo* >());
	return this;
}
void c_R933::mark(){
	c_FunctionInfo::mark();
}
c_R937::c_R937(){
}
c_R937* c_R937::m_new(){
	c_FieldInfo::m_new(String(L"map",3),2,bb_reflection__classes[27]);
	return this;
}
void c_R937::mark(){
	c_FieldInfo::mark();
}
c_R936::c_R936(){
}
c_R936* c_R936::m_new(){
	c_MethodInfo::m_new(String(L"ObjectEnumerator",16),0,bb_reflection__classes[85],Array<c_ClassInfo* >());
	return this;
}
void c_R936::mark(){
	c_MethodInfo::mark();
}
c_R935::c_R935(){
}
c_R935* c_R935::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[27]};
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[70],Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R935::mark(){
	c_FunctionInfo::mark();
}
c_R938::c_R938(){
}
c_R938* c_R938::m_new(){
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[70],Array<c_ClassInfo* >());
	return this;
}
void c_R938::mark(){
	c_FunctionInfo::mark();
}
c_R943::c_R943(){
}
c_R943* c_R943::m_new(){
	c_FieldInfo::m_new(String(L"node",4),2,bb_reflection__classes[68]);
	return this;
}
void c_R943::mark(){
	c_FieldInfo::mark();
}
c_R941::c_R941(){
}
c_R941* c_R941::m_new(){
	c_MethodInfo::m_new(String(L"HasNext",7),0,bb_reflection__boolClass,Array<c_ClassInfo* >());
	return this;
}
void c_R941::mark(){
	c_MethodInfo::mark();
}
c_R942::c_R942(){
}
c_R942* c_R942::m_new(){
	c_MethodInfo::m_new(String(L"NextObject",10),0,bb_reflection__classes[68],Array<c_ClassInfo* >());
	return this;
}
void c_R942::mark(){
	c_MethodInfo::mark();
}
c_R940::c_R940(){
}
c_R940* c_R940::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[68]};
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[71],Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R940::mark(){
	c_FunctionInfo::mark();
}
c_R944::c_R944(){
}
c_R944* c_R944::m_new(){
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[71],Array<c_ClassInfo* >());
	return this;
}
void c_R944::mark(){
	c_FunctionInfo::mark();
}
c_R953::c_R953(){
}
c_R953* c_R953::m_new(){
	c_FieldInfo::m_new(String(L"key",3),2,bb_reflection__floatClass);
	return this;
}
void c_R953::mark(){
	c_FieldInfo::mark();
}
c_R954::c_R954(){
}
c_R954* c_R954::m_new(){
	c_FieldInfo::m_new(String(L"value",5),2,bb_reflection__classes[0]);
	return this;
}
void c_R954::mark(){
	c_FieldInfo::mark();
}
c_R955::c_R955(){
}
c_R955* c_R955::m_new(){
	c_FieldInfo::m_new(String(L"color",5),2,bb_reflection__intClass);
	return this;
}
void c_R955::mark(){
	c_FieldInfo::mark();
}
c_R956::c_R956(){
}
c_R956* c_R956::m_new(){
	c_FieldInfo::m_new(String(L"parent",6),2,bb_reflection__classes[72]);
	return this;
}
void c_R956::mark(){
	c_FieldInfo::mark();
}
c_R957::c_R957(){
}
c_R957* c_R957::m_new(){
	c_FieldInfo::m_new(String(L"left",4),2,bb_reflection__classes[72]);
	return this;
}
void c_R957::mark(){
	c_FieldInfo::mark();
}
c_R958::c_R958(){
}
c_R958* c_R958::m_new(){
	c_FieldInfo::m_new(String(L"right",5),2,bb_reflection__classes[72]);
	return this;
}
void c_R958::mark(){
	c_FieldInfo::mark();
}
c_R947::c_R947(){
}
c_R947* c_R947::m_new(){
	c_ClassInfo* t_[]={bb_reflection__intClass};
	c_MethodInfo::m_new(String(L"Count",5),0,bb_reflection__intClass,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R947::mark(){
	c_MethodInfo::mark();
}
c_R948::c_R948(){
}
c_R948* c_R948::m_new(){
	c_MethodInfo::m_new(String(L"Key",3),0,bb_reflection__floatClass,Array<c_ClassInfo* >());
	return this;
}
void c_R948::mark(){
	c_MethodInfo::mark();
}
c_R949::c_R949(){
}
c_R949* c_R949::m_new(){
	c_MethodInfo::m_new(String(L"Value",5),0,bb_reflection__classes[0],Array<c_ClassInfo* >());
	return this;
}
void c_R949::mark(){
	c_MethodInfo::mark();
}
c_R950::c_R950(){
}
c_R950* c_R950::m_new(){
	c_MethodInfo::m_new(String(L"NextNode",8),0,bb_reflection__classes[72],Array<c_ClassInfo* >());
	return this;
}
void c_R950::mark(){
	c_MethodInfo::mark();
}
c_R951::c_R951(){
}
c_R951* c_R951::m_new(){
	c_MethodInfo::m_new(String(L"PrevNode",8),0,bb_reflection__classes[72],Array<c_ClassInfo* >());
	return this;
}
void c_R951::mark(){
	c_MethodInfo::mark();
}
c_R952::c_R952(){
}
c_R952* c_R952::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[72]};
	c_MethodInfo::m_new(String(L"Copy",4),0,bb_reflection__classes[72],Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R952::mark(){
	c_MethodInfo::mark();
}
c_R946::c_R946(){
}
c_R946* c_R946::m_new(){
	c_ClassInfo* t_[]={bb_reflection__floatClass,bb_reflection__classes[0],bb_reflection__intClass,bb_reflection__classes[72]};
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[72],Array<c_ClassInfo* >(t_,4));
	return this;
}
void c_R946::mark(){
	c_FunctionInfo::mark();
}
c_R959::c_R959(){
}
c_R959* c_R959::m_new(){
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[72],Array<c_ClassInfo* >());
	return this;
}
void c_R959::mark(){
	c_FunctionInfo::mark();
}
c_R963::c_R963(){
}
c_R963* c_R963::m_new(){
	c_FieldInfo::m_new(String(L"map",3),2,bb_reflection__classes[31]);
	return this;
}
void c_R963::mark(){
	c_FieldInfo::mark();
}
c_R962::c_R962(){
}
c_R962* c_R962::m_new(){
	c_MethodInfo::m_new(String(L"ObjectEnumerator",16),0,bb_reflection__classes[83],Array<c_ClassInfo* >());
	return this;
}
void c_R962::mark(){
	c_MethodInfo::mark();
}
c_R961::c_R961(){
}
c_R961* c_R961::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[31]};
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[73],Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R961::mark(){
	c_FunctionInfo::mark();
}
c_R964::c_R964(){
}
c_R964* c_R964::m_new(){
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[73],Array<c_ClassInfo* >());
	return this;
}
void c_R964::mark(){
	c_FunctionInfo::mark();
}
c_R968::c_R968(){
}
c_R968* c_R968::m_new(){
	c_FieldInfo::m_new(String(L"map",3),2,bb_reflection__classes[31]);
	return this;
}
void c_R968::mark(){
	c_FieldInfo::mark();
}
c_R967::c_R967(){
}
c_R967* c_R967::m_new(){
	c_MethodInfo::m_new(String(L"ObjectEnumerator",16),0,bb_reflection__classes[86],Array<c_ClassInfo* >());
	return this;
}
void c_R967::mark(){
	c_MethodInfo::mark();
}
c_R966::c_R966(){
}
c_R966* c_R966::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[31]};
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[74],Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R966::mark(){
	c_FunctionInfo::mark();
}
c_R969::c_R969(){
}
c_R969* c_R969::m_new(){
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[74],Array<c_ClassInfo* >());
	return this;
}
void c_R969::mark(){
	c_FunctionInfo::mark();
}
c_R974::c_R974(){
}
c_R974* c_R974::m_new(){
	c_FieldInfo::m_new(String(L"node",4),2,bb_reflection__classes[72]);
	return this;
}
void c_R974::mark(){
	c_FieldInfo::mark();
}
c_R972::c_R972(){
}
c_R972* c_R972::m_new(){
	c_MethodInfo::m_new(String(L"HasNext",7),0,bb_reflection__boolClass,Array<c_ClassInfo* >());
	return this;
}
void c_R972::mark(){
	c_MethodInfo::mark();
}
c_R973::c_R973(){
}
c_R973* c_R973::m_new(){
	c_MethodInfo::m_new(String(L"NextObject",10),0,bb_reflection__classes[72],Array<c_ClassInfo* >());
	return this;
}
void c_R973::mark(){
	c_MethodInfo::mark();
}
c_R971::c_R971(){
}
c_R971* c_R971::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[72]};
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[75],Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R971::mark(){
	c_FunctionInfo::mark();
}
c_R975::c_R975(){
}
c_R975* c_R975::m_new(){
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[75],Array<c_ClassInfo* >());
	return this;
}
void c_R975::mark(){
	c_FunctionInfo::mark();
}
c_R984::c_R984(){
}
c_R984* c_R984::m_new(){
	c_FieldInfo::m_new(String(L"key",3),2,bb_reflection__stringClass);
	return this;
}
void c_R984::mark(){
	c_FieldInfo::mark();
}
c_R985::c_R985(){
}
c_R985* c_R985::m_new(){
	c_FieldInfo::m_new(String(L"value",5),2,bb_reflection__classes[0]);
	return this;
}
void c_R985::mark(){
	c_FieldInfo::mark();
}
c_R986::c_R986(){
}
c_R986* c_R986::m_new(){
	c_FieldInfo::m_new(String(L"color",5),2,bb_reflection__intClass);
	return this;
}
void c_R986::mark(){
	c_FieldInfo::mark();
}
c_R987::c_R987(){
}
c_R987* c_R987::m_new(){
	c_FieldInfo::m_new(String(L"parent",6),2,bb_reflection__classes[76]);
	return this;
}
void c_R987::mark(){
	c_FieldInfo::mark();
}
c_R988::c_R988(){
}
c_R988* c_R988::m_new(){
	c_FieldInfo::m_new(String(L"left",4),2,bb_reflection__classes[76]);
	return this;
}
void c_R988::mark(){
	c_FieldInfo::mark();
}
c_R989::c_R989(){
}
c_R989* c_R989::m_new(){
	c_FieldInfo::m_new(String(L"right",5),2,bb_reflection__classes[76]);
	return this;
}
void c_R989::mark(){
	c_FieldInfo::mark();
}
c_R978::c_R978(){
}
c_R978* c_R978::m_new(){
	c_ClassInfo* t_[]={bb_reflection__intClass};
	c_MethodInfo::m_new(String(L"Count",5),0,bb_reflection__intClass,Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R978::mark(){
	c_MethodInfo::mark();
}
c_R979::c_R979(){
}
c_R979* c_R979::m_new(){
	c_MethodInfo::m_new(String(L"Key",3),0,bb_reflection__stringClass,Array<c_ClassInfo* >());
	return this;
}
void c_R979::mark(){
	c_MethodInfo::mark();
}
c_R980::c_R980(){
}
c_R980* c_R980::m_new(){
	c_MethodInfo::m_new(String(L"Value",5),0,bb_reflection__classes[0],Array<c_ClassInfo* >());
	return this;
}
void c_R980::mark(){
	c_MethodInfo::mark();
}
c_R981::c_R981(){
}
c_R981* c_R981::m_new(){
	c_MethodInfo::m_new(String(L"NextNode",8),0,bb_reflection__classes[76],Array<c_ClassInfo* >());
	return this;
}
void c_R981::mark(){
	c_MethodInfo::mark();
}
c_R982::c_R982(){
}
c_R982* c_R982::m_new(){
	c_MethodInfo::m_new(String(L"PrevNode",8),0,bb_reflection__classes[76],Array<c_ClassInfo* >());
	return this;
}
void c_R982::mark(){
	c_MethodInfo::mark();
}
c_R983::c_R983(){
}
c_R983* c_R983::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[76]};
	c_MethodInfo::m_new(String(L"Copy",4),0,bb_reflection__classes[76],Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R983::mark(){
	c_MethodInfo::mark();
}
c_R977::c_R977(){
}
c_R977* c_R977::m_new(){
	c_ClassInfo* t_[]={bb_reflection__stringClass,bb_reflection__classes[0],bb_reflection__intClass,bb_reflection__classes[76]};
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[76],Array<c_ClassInfo* >(t_,4));
	return this;
}
void c_R977::mark(){
	c_FunctionInfo::mark();
}
c_R990::c_R990(){
}
c_R990* c_R990::m_new(){
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[76],Array<c_ClassInfo* >());
	return this;
}
void c_R990::mark(){
	c_FunctionInfo::mark();
}
c_R994::c_R994(){
}
c_R994* c_R994::m_new(){
	c_FieldInfo::m_new(String(L"map",3),2,bb_reflection__classes[35]);
	return this;
}
void c_R994::mark(){
	c_FieldInfo::mark();
}
c_R993::c_R993(){
}
c_R993* c_R993::m_new(){
	c_MethodInfo::m_new(String(L"ObjectEnumerator",16),0,bb_reflection__classes[84],Array<c_ClassInfo* >());
	return this;
}
void c_R993::mark(){
	c_MethodInfo::mark();
}
c_R992::c_R992(){
}
c_R992* c_R992::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[35]};
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[77],Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R992::mark(){
	c_FunctionInfo::mark();
}
c_R995::c_R995(){
}
c_R995* c_R995::m_new(){
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[77],Array<c_ClassInfo* >());
	return this;
}
void c_R995::mark(){
	c_FunctionInfo::mark();
}
c_R999::c_R999(){
}
c_R999* c_R999::m_new(){
	c_FieldInfo::m_new(String(L"map",3),2,bb_reflection__classes[35]);
	return this;
}
void c_R999::mark(){
	c_FieldInfo::mark();
}
c_R998::c_R998(){
}
c_R998* c_R998::m_new(){
	c_MethodInfo::m_new(String(L"ObjectEnumerator",16),0,bb_reflection__classes[87],Array<c_ClassInfo* >());
	return this;
}
void c_R998::mark(){
	c_MethodInfo::mark();
}
c_R997::c_R997(){
}
c_R997* c_R997::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[35]};
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[78],Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R997::mark(){
	c_FunctionInfo::mark();
}
c_R1000::c_R1000(){
}
c_R1000* c_R1000::m_new(){
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[78],Array<c_ClassInfo* >());
	return this;
}
void c_R1000::mark(){
	c_FunctionInfo::mark();
}
c_R1005::c_R1005(){
}
c_R1005* c_R1005::m_new(){
	c_FieldInfo::m_new(String(L"node",4),2,bb_reflection__classes[76]);
	return this;
}
void c_R1005::mark(){
	c_FieldInfo::mark();
}
c_R1003::c_R1003(){
}
c_R1003* c_R1003::m_new(){
	c_MethodInfo::m_new(String(L"HasNext",7),0,bb_reflection__boolClass,Array<c_ClassInfo* >());
	return this;
}
void c_R1003::mark(){
	c_MethodInfo::mark();
}
c_R1004::c_R1004(){
}
c_R1004* c_R1004::m_new(){
	c_MethodInfo::m_new(String(L"NextObject",10),0,bb_reflection__classes[76],Array<c_ClassInfo* >());
	return this;
}
void c_R1004::mark(){
	c_MethodInfo::mark();
}
c_R1002::c_R1002(){
}
c_R1002* c_R1002::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[76]};
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[79],Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R1002::mark(){
	c_FunctionInfo::mark();
}
c_R1006::c_R1006(){
}
c_R1006* c_R1006::m_new(){
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[79],Array<c_ClassInfo* >());
	return this;
}
void c_R1006::mark(){
	c_FunctionInfo::mark();
}
c_R1010::c_R1010(){
}
c_R1010* c_R1010::m_new(){
	c_FieldInfo::m_new(String(L"map",3),2,bb_reflection__classes[48]);
	return this;
}
void c_R1010::mark(){
	c_FieldInfo::mark();
}
c_R1009::c_R1009(){
}
c_R1009* c_R1009::m_new(){
	c_MethodInfo::m_new(String(L"ObjectEnumerator",16),0,bb_reflection__classes[88],Array<c_ClassInfo* >());
	return this;
}
void c_R1009::mark(){
	c_MethodInfo::mark();
}
c_R1008::c_R1008(){
}
c_R1008* c_R1008::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[48]};
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[80],Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R1008::mark(){
	c_FunctionInfo::mark();
}
c_R1011::c_R1011(){
}
c_R1011* c_R1011::m_new(){
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[80],Array<c_ClassInfo* >());
	return this;
}
void c_R1011::mark(){
	c_FunctionInfo::mark();
}
c_R1016::c_R1016(){
}
c_R1016* c_R1016::m_new(){
	c_FieldInfo::m_new(String(L"node",4),2,bb_reflection__classes[50]);
	return this;
}
void c_R1016::mark(){
	c_FieldInfo::mark();
}
c_R1014::c_R1014(){
}
c_R1014* c_R1014::m_new(){
	c_MethodInfo::m_new(String(L"HasNext",7),0,bb_reflection__boolClass,Array<c_ClassInfo* >());
	return this;
}
void c_R1014::mark(){
	c_MethodInfo::mark();
}
c_R1015::c_R1015(){
}
c_R1015* c_R1015::m_new(){
	c_MethodInfo::m_new(String(L"NextObject",10),0,bb_reflection__classes[50],Array<c_ClassInfo* >());
	return this;
}
void c_R1015::mark(){
	c_MethodInfo::mark();
}
c_R1013::c_R1013(){
}
c_R1013* c_R1013::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[50]};
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[81],Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R1013::mark(){
	c_FunctionInfo::mark();
}
c_R1017::c_R1017(){
}
c_R1017* c_R1017::m_new(){
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[81],Array<c_ClassInfo* >());
	return this;
}
void c_R1017::mark(){
	c_FunctionInfo::mark();
}
c_R1022::c_R1022(){
}
c_R1022* c_R1022::m_new(){
	c_FieldInfo::m_new(String(L"node",4),2,bb_reflection__classes[68]);
	return this;
}
void c_R1022::mark(){
	c_FieldInfo::mark();
}
c_R1020::c_R1020(){
}
c_R1020* c_R1020::m_new(){
	c_MethodInfo::m_new(String(L"HasNext",7),0,bb_reflection__boolClass,Array<c_ClassInfo* >());
	return this;
}
void c_R1020::mark(){
	c_MethodInfo::mark();
}
c_R1021::c_R1021(){
}
c_R1021* c_R1021::m_new(){
	c_MethodInfo::m_new(String(L"NextObject",10),0,bb_reflection__intClass,Array<c_ClassInfo* >());
	return this;
}
void c_R1021::mark(){
	c_MethodInfo::mark();
}
c_R1019::c_R1019(){
}
c_R1019* c_R1019::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[68]};
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[82],Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R1019::mark(){
	c_FunctionInfo::mark();
}
c_R1023::c_R1023(){
}
c_R1023* c_R1023::m_new(){
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[82],Array<c_ClassInfo* >());
	return this;
}
void c_R1023::mark(){
	c_FunctionInfo::mark();
}
c_R1028::c_R1028(){
}
c_R1028* c_R1028::m_new(){
	c_FieldInfo::m_new(String(L"node",4),2,bb_reflection__classes[72]);
	return this;
}
void c_R1028::mark(){
	c_FieldInfo::mark();
}
c_R1026::c_R1026(){
}
c_R1026* c_R1026::m_new(){
	c_MethodInfo::m_new(String(L"HasNext",7),0,bb_reflection__boolClass,Array<c_ClassInfo* >());
	return this;
}
void c_R1026::mark(){
	c_MethodInfo::mark();
}
c_R1027::c_R1027(){
}
c_R1027* c_R1027::m_new(){
	c_MethodInfo::m_new(String(L"NextObject",10),0,bb_reflection__floatClass,Array<c_ClassInfo* >());
	return this;
}
void c_R1027::mark(){
	c_MethodInfo::mark();
}
c_R1025::c_R1025(){
}
c_R1025* c_R1025::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[72]};
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[83],Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R1025::mark(){
	c_FunctionInfo::mark();
}
c_R1029::c_R1029(){
}
c_R1029* c_R1029::m_new(){
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[83],Array<c_ClassInfo* >());
	return this;
}
void c_R1029::mark(){
	c_FunctionInfo::mark();
}
c_R1034::c_R1034(){
}
c_R1034* c_R1034::m_new(){
	c_FieldInfo::m_new(String(L"node",4),2,bb_reflection__classes[76]);
	return this;
}
void c_R1034::mark(){
	c_FieldInfo::mark();
}
c_R1032::c_R1032(){
}
c_R1032* c_R1032::m_new(){
	c_MethodInfo::m_new(String(L"HasNext",7),0,bb_reflection__boolClass,Array<c_ClassInfo* >());
	return this;
}
void c_R1032::mark(){
	c_MethodInfo::mark();
}
c_R1033::c_R1033(){
}
c_R1033* c_R1033::m_new(){
	c_MethodInfo::m_new(String(L"NextObject",10),0,bb_reflection__stringClass,Array<c_ClassInfo* >());
	return this;
}
void c_R1033::mark(){
	c_MethodInfo::mark();
}
c_R1031::c_R1031(){
}
c_R1031* c_R1031::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[76]};
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[84],Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R1031::mark(){
	c_FunctionInfo::mark();
}
c_R1035::c_R1035(){
}
c_R1035* c_R1035::m_new(){
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[84],Array<c_ClassInfo* >());
	return this;
}
void c_R1035::mark(){
	c_FunctionInfo::mark();
}
c_R1040::c_R1040(){
}
c_R1040* c_R1040::m_new(){
	c_FieldInfo::m_new(String(L"node",4),2,bb_reflection__classes[68]);
	return this;
}
void c_R1040::mark(){
	c_FieldInfo::mark();
}
c_R1038::c_R1038(){
}
c_R1038* c_R1038::m_new(){
	c_MethodInfo::m_new(String(L"HasNext",7),0,bb_reflection__boolClass,Array<c_ClassInfo* >());
	return this;
}
void c_R1038::mark(){
	c_MethodInfo::mark();
}
c_R1039::c_R1039(){
}
c_R1039* c_R1039::m_new(){
	c_MethodInfo::m_new(String(L"NextObject",10),0,bb_reflection__classes[0],Array<c_ClassInfo* >());
	return this;
}
void c_R1039::mark(){
	c_MethodInfo::mark();
}
c_R1037::c_R1037(){
}
c_R1037* c_R1037::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[68]};
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[85],Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R1037::mark(){
	c_FunctionInfo::mark();
}
c_R1041::c_R1041(){
}
c_R1041* c_R1041::m_new(){
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[85],Array<c_ClassInfo* >());
	return this;
}
void c_R1041::mark(){
	c_FunctionInfo::mark();
}
c_R1046::c_R1046(){
}
c_R1046* c_R1046::m_new(){
	c_FieldInfo::m_new(String(L"node",4),2,bb_reflection__classes[72]);
	return this;
}
void c_R1046::mark(){
	c_FieldInfo::mark();
}
c_R1044::c_R1044(){
}
c_R1044* c_R1044::m_new(){
	c_MethodInfo::m_new(String(L"HasNext",7),0,bb_reflection__boolClass,Array<c_ClassInfo* >());
	return this;
}
void c_R1044::mark(){
	c_MethodInfo::mark();
}
c_R1045::c_R1045(){
}
c_R1045* c_R1045::m_new(){
	c_MethodInfo::m_new(String(L"NextObject",10),0,bb_reflection__classes[0],Array<c_ClassInfo* >());
	return this;
}
void c_R1045::mark(){
	c_MethodInfo::mark();
}
c_R1043::c_R1043(){
}
c_R1043* c_R1043::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[72]};
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[86],Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R1043::mark(){
	c_FunctionInfo::mark();
}
c_R1047::c_R1047(){
}
c_R1047* c_R1047::m_new(){
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[86],Array<c_ClassInfo* >());
	return this;
}
void c_R1047::mark(){
	c_FunctionInfo::mark();
}
c_R1052::c_R1052(){
}
c_R1052* c_R1052::m_new(){
	c_FieldInfo::m_new(String(L"node",4),2,bb_reflection__classes[76]);
	return this;
}
void c_R1052::mark(){
	c_FieldInfo::mark();
}
c_R1050::c_R1050(){
}
c_R1050* c_R1050::m_new(){
	c_MethodInfo::m_new(String(L"HasNext",7),0,bb_reflection__boolClass,Array<c_ClassInfo* >());
	return this;
}
void c_R1050::mark(){
	c_MethodInfo::mark();
}
c_R1051::c_R1051(){
}
c_R1051* c_R1051::m_new(){
	c_MethodInfo::m_new(String(L"NextObject",10),0,bb_reflection__classes[0],Array<c_ClassInfo* >());
	return this;
}
void c_R1051::mark(){
	c_MethodInfo::mark();
}
c_R1049::c_R1049(){
}
c_R1049* c_R1049::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[76]};
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[87],Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R1049::mark(){
	c_FunctionInfo::mark();
}
c_R1053::c_R1053(){
}
c_R1053* c_R1053::m_new(){
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[87],Array<c_ClassInfo* >());
	return this;
}
void c_R1053::mark(){
	c_FunctionInfo::mark();
}
c_R1058::c_R1058(){
}
c_R1058* c_R1058::m_new(){
	c_FieldInfo::m_new(String(L"node",4),2,bb_reflection__classes[50]);
	return this;
}
void c_R1058::mark(){
	c_FieldInfo::mark();
}
c_R1056::c_R1056(){
}
c_R1056* c_R1056::m_new(){
	c_MethodInfo::m_new(String(L"HasNext",7),0,bb_reflection__boolClass,Array<c_ClassInfo* >());
	return this;
}
void c_R1056::mark(){
	c_MethodInfo::mark();
}
c_R1057::c_R1057(){
}
c_R1057* c_R1057::m_new(){
	c_MethodInfo::m_new(String(L"NextObject",10),0,bb_reflection__stringClass,Array<c_ClassInfo* >());
	return this;
}
void c_R1057::mark(){
	c_MethodInfo::mark();
}
c_R1055::c_R1055(){
}
c_R1055* c_R1055::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[50]};
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[88],Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R1055::mark(){
	c_FunctionInfo::mark();
}
c_R1059::c_R1059(){
}
c_R1059* c_R1059::m_new(){
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[88],Array<c_ClassInfo* >());
	return this;
}
void c_R1059::mark(){
	c_FunctionInfo::mark();
}
c_R1064::c_R1064(){
}
c_R1064* c_R1064::m_new(){
	c_FieldInfo::m_new(String(L"stack",5),2,bb_reflection__classes[37]);
	return this;
}
void c_R1064::mark(){
	c_FieldInfo::mark();
}
c_R1065::c_R1065(){
}
c_R1065* c_R1065::m_new(){
	c_FieldInfo::m_new(String(L"index",5),2,bb_reflection__intClass);
	return this;
}
void c_R1065::mark(){
	c_FieldInfo::mark();
}
c_R1062::c_R1062(){
}
c_R1062* c_R1062::m_new(){
	c_MethodInfo::m_new(String(L"HasNext",7),0,bb_reflection__boolClass,Array<c_ClassInfo* >());
	return this;
}
void c_R1062::mark(){
	c_MethodInfo::mark();
}
c_R1063::c_R1063(){
}
c_R1063* c_R1063::m_new(){
	c_MethodInfo::m_new(String(L"NextObject",10),0,bb_reflection__intClass,Array<c_ClassInfo* >());
	return this;
}
void c_R1063::mark(){
	c_MethodInfo::mark();
}
c_R1061::c_R1061(){
}
c_R1061* c_R1061::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[37]};
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[89],Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R1061::mark(){
	c_FunctionInfo::mark();
}
c_R1066::c_R1066(){
}
c_R1066* c_R1066::m_new(){
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[89],Array<c_ClassInfo* >());
	return this;
}
void c_R1066::mark(){
	c_FunctionInfo::mark();
}
c_R1070::c_R1070(){
}
c_R1070* c_R1070::m_new(){
	c_FieldInfo::m_new(String(L"stack",5),2,bb_reflection__classes[37]);
	return this;
}
void c_R1070::mark(){
	c_FieldInfo::mark();
}
c_R1069::c_R1069(){
}
c_R1069* c_R1069::m_new(){
	c_MethodInfo::m_new(String(L"ObjectEnumerator",16),0,bb_reflection__classes[95],Array<c_ClassInfo* >());
	return this;
}
void c_R1069::mark(){
	c_MethodInfo::mark();
}
c_R1068::c_R1068(){
}
c_R1068* c_R1068::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[37]};
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[90],Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R1068::mark(){
	c_FunctionInfo::mark();
}
c_R1071::c_R1071(){
}
c_R1071* c_R1071::m_new(){
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[90],Array<c_ClassInfo* >());
	return this;
}
void c_R1071::mark(){
	c_FunctionInfo::mark();
}
c_R1076::c_R1076(){
}
c_R1076* c_R1076::m_new(){
	c_FieldInfo::m_new(String(L"stack",5),2,bb_reflection__classes[39]);
	return this;
}
void c_R1076::mark(){
	c_FieldInfo::mark();
}
c_R1077::c_R1077(){
}
c_R1077* c_R1077::m_new(){
	c_FieldInfo::m_new(String(L"index",5),2,bb_reflection__intClass);
	return this;
}
void c_R1077::mark(){
	c_FieldInfo::mark();
}
c_R1074::c_R1074(){
}
c_R1074* c_R1074::m_new(){
	c_MethodInfo::m_new(String(L"HasNext",7),0,bb_reflection__boolClass,Array<c_ClassInfo* >());
	return this;
}
void c_R1074::mark(){
	c_MethodInfo::mark();
}
c_R1075::c_R1075(){
}
c_R1075* c_R1075::m_new(){
	c_MethodInfo::m_new(String(L"NextObject",10),0,bb_reflection__floatClass,Array<c_ClassInfo* >());
	return this;
}
void c_R1075::mark(){
	c_MethodInfo::mark();
}
c_R1073::c_R1073(){
}
c_R1073* c_R1073::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[39]};
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[91],Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R1073::mark(){
	c_FunctionInfo::mark();
}
c_R1078::c_R1078(){
}
c_R1078* c_R1078::m_new(){
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[91],Array<c_ClassInfo* >());
	return this;
}
void c_R1078::mark(){
	c_FunctionInfo::mark();
}
c_R1082::c_R1082(){
}
c_R1082* c_R1082::m_new(){
	c_FieldInfo::m_new(String(L"stack",5),2,bb_reflection__classes[39]);
	return this;
}
void c_R1082::mark(){
	c_FieldInfo::mark();
}
c_R1081::c_R1081(){
}
c_R1081* c_R1081::m_new(){
	c_MethodInfo::m_new(String(L"ObjectEnumerator",16),0,bb_reflection__classes[96],Array<c_ClassInfo* >());
	return this;
}
void c_R1081::mark(){
	c_MethodInfo::mark();
}
c_R1080::c_R1080(){
}
c_R1080* c_R1080::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[39]};
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[92],Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R1080::mark(){
	c_FunctionInfo::mark();
}
c_R1083::c_R1083(){
}
c_R1083* c_R1083::m_new(){
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[92],Array<c_ClassInfo* >());
	return this;
}
void c_R1083::mark(){
	c_FunctionInfo::mark();
}
c_R1088::c_R1088(){
}
c_R1088* c_R1088::m_new(){
	c_FieldInfo::m_new(String(L"stack",5),2,bb_reflection__classes[41]);
	return this;
}
void c_R1088::mark(){
	c_FieldInfo::mark();
}
c_R1089::c_R1089(){
}
c_R1089* c_R1089::m_new(){
	c_FieldInfo::m_new(String(L"index",5),2,bb_reflection__intClass);
	return this;
}
void c_R1089::mark(){
	c_FieldInfo::mark();
}
c_R1086::c_R1086(){
}
c_R1086* c_R1086::m_new(){
	c_MethodInfo::m_new(String(L"HasNext",7),0,bb_reflection__boolClass,Array<c_ClassInfo* >());
	return this;
}
void c_R1086::mark(){
	c_MethodInfo::mark();
}
c_R1087::c_R1087(){
}
c_R1087* c_R1087::m_new(){
	c_MethodInfo::m_new(String(L"NextObject",10),0,bb_reflection__stringClass,Array<c_ClassInfo* >());
	return this;
}
void c_R1087::mark(){
	c_MethodInfo::mark();
}
c_R1085::c_R1085(){
}
c_R1085* c_R1085::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[41]};
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[93],Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R1085::mark(){
	c_FunctionInfo::mark();
}
c_R1090::c_R1090(){
}
c_R1090* c_R1090::m_new(){
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[93],Array<c_ClassInfo* >());
	return this;
}
void c_R1090::mark(){
	c_FunctionInfo::mark();
}
c_R1094::c_R1094(){
}
c_R1094* c_R1094::m_new(){
	c_FieldInfo::m_new(String(L"stack",5),2,bb_reflection__classes[41]);
	return this;
}
void c_R1094::mark(){
	c_FieldInfo::mark();
}
c_R1093::c_R1093(){
}
c_R1093* c_R1093::m_new(){
	c_MethodInfo::m_new(String(L"ObjectEnumerator",16),0,bb_reflection__classes[97],Array<c_ClassInfo* >());
	return this;
}
void c_R1093::mark(){
	c_MethodInfo::mark();
}
c_R1092::c_R1092(){
}
c_R1092* c_R1092::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[41]};
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[94],Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R1092::mark(){
	c_FunctionInfo::mark();
}
c_R1095::c_R1095(){
}
c_R1095* c_R1095::m_new(){
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[94],Array<c_ClassInfo* >());
	return this;
}
void c_R1095::mark(){
	c_FunctionInfo::mark();
}
c_R1100::c_R1100(){
}
c_R1100* c_R1100::m_new(){
	c_FieldInfo::m_new(String(L"stack",5),2,bb_reflection__classes[37]);
	return this;
}
void c_R1100::mark(){
	c_FieldInfo::mark();
}
c_R1101::c_R1101(){
}
c_R1101* c_R1101::m_new(){
	c_FieldInfo::m_new(String(L"index",5),2,bb_reflection__intClass);
	return this;
}
void c_R1101::mark(){
	c_FieldInfo::mark();
}
c_R1098::c_R1098(){
}
c_R1098* c_R1098::m_new(){
	c_MethodInfo::m_new(String(L"HasNext",7),0,bb_reflection__boolClass,Array<c_ClassInfo* >());
	return this;
}
void c_R1098::mark(){
	c_MethodInfo::mark();
}
c_R1099::c_R1099(){
}
c_R1099* c_R1099::m_new(){
	c_MethodInfo::m_new(String(L"NextObject",10),0,bb_reflection__intClass,Array<c_ClassInfo* >());
	return this;
}
void c_R1099::mark(){
	c_MethodInfo::mark();
}
c_R1097::c_R1097(){
}
c_R1097* c_R1097::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[37]};
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[95],Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R1097::mark(){
	c_FunctionInfo::mark();
}
c_R1102::c_R1102(){
}
c_R1102* c_R1102::m_new(){
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[95],Array<c_ClassInfo* >());
	return this;
}
void c_R1102::mark(){
	c_FunctionInfo::mark();
}
c_R1107::c_R1107(){
}
c_R1107* c_R1107::m_new(){
	c_FieldInfo::m_new(String(L"stack",5),2,bb_reflection__classes[39]);
	return this;
}
void c_R1107::mark(){
	c_FieldInfo::mark();
}
c_R1108::c_R1108(){
}
c_R1108* c_R1108::m_new(){
	c_FieldInfo::m_new(String(L"index",5),2,bb_reflection__intClass);
	return this;
}
void c_R1108::mark(){
	c_FieldInfo::mark();
}
c_R1105::c_R1105(){
}
c_R1105* c_R1105::m_new(){
	c_MethodInfo::m_new(String(L"HasNext",7),0,bb_reflection__boolClass,Array<c_ClassInfo* >());
	return this;
}
void c_R1105::mark(){
	c_MethodInfo::mark();
}
c_R1106::c_R1106(){
}
c_R1106* c_R1106::m_new(){
	c_MethodInfo::m_new(String(L"NextObject",10),0,bb_reflection__floatClass,Array<c_ClassInfo* >());
	return this;
}
void c_R1106::mark(){
	c_MethodInfo::mark();
}
c_R1104::c_R1104(){
}
c_R1104* c_R1104::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[39]};
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[96],Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R1104::mark(){
	c_FunctionInfo::mark();
}
c_R1109::c_R1109(){
}
c_R1109* c_R1109::m_new(){
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[96],Array<c_ClassInfo* >());
	return this;
}
void c_R1109::mark(){
	c_FunctionInfo::mark();
}
c_R1114::c_R1114(){
}
c_R1114* c_R1114::m_new(){
	c_FieldInfo::m_new(String(L"stack",5),2,bb_reflection__classes[41]);
	return this;
}
void c_R1114::mark(){
	c_FieldInfo::mark();
}
c_R1115::c_R1115(){
}
c_R1115* c_R1115::m_new(){
	c_FieldInfo::m_new(String(L"index",5),2,bb_reflection__intClass);
	return this;
}
void c_R1115::mark(){
	c_FieldInfo::mark();
}
c_R1112::c_R1112(){
}
c_R1112* c_R1112::m_new(){
	c_MethodInfo::m_new(String(L"HasNext",7),0,bb_reflection__boolClass,Array<c_ClassInfo* >());
	return this;
}
void c_R1112::mark(){
	c_MethodInfo::mark();
}
c_R1113::c_R1113(){
}
c_R1113* c_R1113::m_new(){
	c_MethodInfo::m_new(String(L"NextObject",10),0,bb_reflection__stringClass,Array<c_ClassInfo* >());
	return this;
}
void c_R1113::mark(){
	c_MethodInfo::mark();
}
c_R1111::c_R1111(){
}
c_R1111* c_R1111::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[41]};
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[97],Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R1111::mark(){
	c_FunctionInfo::mark();
}
c_R1116::c_R1116(){
}
c_R1116* c_R1116::m_new(){
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[97],Array<c_ClassInfo* >());
	return this;
}
void c_R1116::mark(){
	c_FunctionInfo::mark();
}
c_R1118::c_R1118(){
}
c_R1118* c_R1118::m_new(){
	c_FieldInfo::m_new(String(L"value",5),0,bb_reflection__classes[98]);
	return this;
}
void c_R1118::mark(){
	c_FieldInfo::mark();
}
c_R1120::c_R1120(){
}
c_R1120* c_R1120::m_new(){
	c_MethodInfo::m_new(String(L"ToArray",7),0,bb_reflection__classes[98],Array<c_ClassInfo* >());
	return this;
}
void c_R1120::mark(){
	c_MethodInfo::mark();
}
c_R1119::c_R1119(){
}
c_R1119* c_R1119::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[98]};
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[98],Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R1119::mark(){
	c_FunctionInfo::mark();
}
c_R1121::c_R1121(){
}
c_R1121* c_R1121::m_new(){
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[98],Array<c_ClassInfo* >());
	return this;
}
void c_R1121::mark(){
	c_FunctionInfo::mark();
}
c_R1123::c_R1123(){
}
c_R1123* c_R1123::m_new(){
	c_FieldInfo::m_new(String(L"value",5),0,bb_reflection__classes[99]);
	return this;
}
void c_R1123::mark(){
	c_FieldInfo::mark();
}
c_R1125::c_R1125(){
}
c_R1125* c_R1125::m_new(){
	c_MethodInfo::m_new(String(L"ToArray",7),0,bb_reflection__classes[99],Array<c_ClassInfo* >());
	return this;
}
void c_R1125::mark(){
	c_MethodInfo::mark();
}
c_R1124::c_R1124(){
}
c_R1124* c_R1124::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[99]};
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[99],Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R1124::mark(){
	c_FunctionInfo::mark();
}
c_R1126::c_R1126(){
}
c_R1126* c_R1126::m_new(){
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[99],Array<c_ClassInfo* >());
	return this;
}
void c_R1126::mark(){
	c_FunctionInfo::mark();
}
c_R1128::c_R1128(){
}
c_R1128* c_R1128::m_new(){
	c_FieldInfo::m_new(String(L"value",5),0,bb_reflection__classes[100]);
	return this;
}
void c_R1128::mark(){
	c_FieldInfo::mark();
}
c_R1130::c_R1130(){
}
c_R1130* c_R1130::m_new(){
	c_MethodInfo::m_new(String(L"ToArray",7),0,bb_reflection__classes[100],Array<c_ClassInfo* >());
	return this;
}
void c_R1130::mark(){
	c_MethodInfo::mark();
}
c_R1129::c_R1129(){
}
c_R1129* c_R1129::m_new(){
	c_ClassInfo* t_[]={bb_reflection__classes[100]};
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[100],Array<c_ClassInfo* >(t_,1));
	return this;
}
void c_R1129::mark(){
	c_FunctionInfo::mark();
}
c_R1131::c_R1131(){
}
c_R1131* c_R1131::m_new(){
	c_FunctionInfo::m_new(String(L"new",3),0,bb_reflection__classes[100],Array<c_ClassInfo* >());
	return this;
}
void c_R1131::mark(){
	c_FunctionInfo::mark();
}
int bb_graphics_SetGraphicsDevice(gxtkGraphics* t_dev){
	gc_assign(bb_graphics_device,t_dev);
	return 0;
}
int bb_graphics_SetFont(c_Image* t_font,int t_firstChar){
	if(!((t_font)!=0)){
		if(!((bb_graphics_context->m_defaultFont)!=0)){
			gc_assign(bb_graphics_context->m_defaultFont,bb_graphics_LoadImage(String(L"mojo_font.png",13),96,2));
		}
		t_font=bb_graphics_context->m_defaultFont;
		t_firstChar=32;
	}
	gc_assign(bb_graphics_context->m_font,t_font);
	bb_graphics_context->m_firstChar=t_firstChar;
	return 0;
}
gxtkAudio* bb_audio_device;
int bb_audio_SetAudioDevice(gxtkAudio* t_dev){
	gc_assign(bb_audio_device,t_dev);
	return 0;
}
c_InputDevice::c_InputDevice(){
	m__joyStates=Array<c_JoyState* >(4);
	m__keyDown=Array<bool >(512);
	m__keyHitPut=0;
	m__keyHitQueue=Array<int >(33);
	m__keyHit=Array<int >(512);
	m__charGet=0;
	m__charPut=0;
	m__charQueue=Array<int >(32);
	m__mouseX=FLOAT(.0);
	m__mouseY=FLOAT(.0);
	m__touchX=Array<Float >(32);
	m__touchY=Array<Float >(32);
	m__accelX=FLOAT(.0);
	m__accelY=FLOAT(.0);
	m__accelZ=FLOAT(.0);
}
c_InputDevice* c_InputDevice::m_new(){
	for(int t_i=0;t_i<4;t_i=t_i+1){
		gc_assign(m__joyStates[t_i],(new c_JoyState)->m_new());
	}
	return this;
}
void c_InputDevice::p_PutKeyHit(int t_key){
	if(m__keyHitPut==m__keyHitQueue.Length()){
		return;
	}
	m__keyHit[t_key]+=1;
	m__keyHitQueue[m__keyHitPut]=t_key;
	m__keyHitPut+=1;
}
void c_InputDevice::p_BeginUpdate(){
	for(int t_i=0;t_i<4;t_i=t_i+1){
		c_JoyState* t_state=m__joyStates[t_i];
		if(!BBGame::Game()->PollJoystick(t_i,t_state->m_joyx,t_state->m_joyy,t_state->m_joyz,t_state->m_buttons)){
			break;
		}
		for(int t_j=0;t_j<32;t_j=t_j+1){
			int t_key=256+t_i*32+t_j;
			if(t_state->m_buttons[t_j]){
				if(!m__keyDown[t_key]){
					m__keyDown[t_key]=true;
					p_PutKeyHit(t_key);
				}
			}else{
				m__keyDown[t_key]=false;
			}
		}
	}
}
void c_InputDevice::p_EndUpdate(){
	for(int t_i=0;t_i<m__keyHitPut;t_i=t_i+1){
		m__keyHit[m__keyHitQueue[t_i]]=0;
	}
	m__keyHitPut=0;
	m__charGet=0;
	m__charPut=0;
}
void c_InputDevice::p_KeyEvent(int t_event,int t_data){
	int t_1=t_event;
	if(t_1==1){
		if(!m__keyDown[t_data]){
			m__keyDown[t_data]=true;
			p_PutKeyHit(t_data);
			if(t_data==1){
				m__keyDown[384]=true;
				p_PutKeyHit(384);
			}else{
				if(t_data==384){
					m__keyDown[1]=true;
					p_PutKeyHit(1);
				}
			}
		}
	}else{
		if(t_1==2){
			if(m__keyDown[t_data]){
				m__keyDown[t_data]=false;
				if(t_data==1){
					m__keyDown[384]=false;
				}else{
					if(t_data==384){
						m__keyDown[1]=false;
					}
				}
			}
		}else{
			if(t_1==3){
				if(m__charPut<m__charQueue.Length()){
					m__charQueue[m__charPut]=t_data;
					m__charPut+=1;
				}
			}
		}
	}
}
void c_InputDevice::p_MouseEvent(int t_event,int t_data,Float t_x,Float t_y){
	int t_2=t_event;
	if(t_2==4){
		p_KeyEvent(1,1+t_data);
	}else{
		if(t_2==5){
			p_KeyEvent(2,1+t_data);
			return;
		}else{
			if(t_2==6){
			}else{
				return;
			}
		}
	}
	m__mouseX=t_x;
	m__mouseY=t_y;
	m__touchX[0]=t_x;
	m__touchY[0]=t_y;
}
void c_InputDevice::p_TouchEvent(int t_event,int t_data,Float t_x,Float t_y){
	int t_3=t_event;
	if(t_3==7){
		p_KeyEvent(1,384+t_data);
	}else{
		if(t_3==8){
			p_KeyEvent(2,384+t_data);
			return;
		}else{
			if(t_3==9){
			}else{
				return;
			}
		}
	}
	m__touchX[t_data]=t_x;
	m__touchY[t_data]=t_y;
	if(t_data==0){
		m__mouseX=t_x;
		m__mouseY=t_y;
	}
}
void c_InputDevice::p_MotionEvent(int t_event,int t_data,Float t_x,Float t_y,Float t_z){
	int t_4=t_event;
	if(t_4==10){
	}else{
		return;
	}
	m__accelX=t_x;
	m__accelY=t_y;
	m__accelZ=t_z;
}
int c_InputDevice::p_KeyHit(int t_key){
	if(t_key>0 && t_key<512){
		return m__keyHit[t_key];
	}
	return 0;
}
void c_InputDevice::mark(){
	Object::mark();
	gc_mark_q(m__joyStates);
	gc_mark_q(m__keyDown);
	gc_mark_q(m__keyHitQueue);
	gc_mark_q(m__keyHit);
	gc_mark_q(m__charQueue);
	gc_mark_q(m__touchX);
	gc_mark_q(m__touchY);
}
c_JoyState::c_JoyState(){
	m_joyx=Array<Float >(2);
	m_joyy=Array<Float >(2);
	m_joyz=Array<Float >(2);
	m_buttons=Array<bool >(32);
}
c_JoyState* c_JoyState::m_new(){
	return this;
}
void c_JoyState::mark(){
	Object::mark();
	gc_mark_q(m_joyx);
	gc_mark_q(m_joyy);
	gc_mark_q(m_joyz);
	gc_mark_q(m_buttons);
}
c_InputDevice* bb_input_device;
int bb_input_SetInputDevice(c_InputDevice* t_dev){
	gc_assign(bb_input_device,t_dev);
	return 0;
}
int bb_app__devWidth;
int bb_app__devHeight;
void bb_app_ValidateDeviceWindow(bool t_notifyApp){
	int t_w=bb_app__game->GetDeviceWidth();
	int t_h=bb_app__game->GetDeviceHeight();
	if(t_w==bb_app__devWidth && t_h==bb_app__devHeight){
		return;
	}
	bb_app__devWidth=t_w;
	bb_app__devHeight=t_h;
	if(t_notifyApp){
		bb_app__app->p_OnResize();
	}
}
c_DisplayMode::c_DisplayMode(){
	m__width=0;
	m__height=0;
}
c_DisplayMode* c_DisplayMode::m_new(int t_width,int t_height){
	m__width=t_width;
	m__height=t_height;
	return this;
}
c_DisplayMode* c_DisplayMode::m_new2(){
	return this;
}
void c_DisplayMode::mark(){
	Object::mark();
}
c_Map6::c_Map6(){
	m_root=0;
}
c_Map6* c_Map6::m_new(){
	return this;
}
c_Node9* c_Map6::p_FindNode(int t_key){
	c_Node9* t_node=m_root;
	while((t_node)!=0){
		int t_cmp=p_Compare4(t_key,t_node->m_key);
		if(t_cmp>0){
			t_node=t_node->m_right;
		}else{
			if(t_cmp<0){
				t_node=t_node->m_left;
			}else{
				return t_node;
			}
		}
	}
	return t_node;
}
bool c_Map6::p_Contains(int t_key){
	return p_FindNode(t_key)!=0;
}
int c_Map6::p_RotateLeft7(c_Node9* t_node){
	c_Node9* t_child=t_node->m_right;
	gc_assign(t_node->m_right,t_child->m_left);
	if((t_child->m_left)!=0){
		gc_assign(t_child->m_left->m_parent,t_node);
	}
	gc_assign(t_child->m_parent,t_node->m_parent);
	if((t_node->m_parent)!=0){
		if(t_node==t_node->m_parent->m_left){
			gc_assign(t_node->m_parent->m_left,t_child);
		}else{
			gc_assign(t_node->m_parent->m_right,t_child);
		}
	}else{
		gc_assign(m_root,t_child);
	}
	gc_assign(t_child->m_left,t_node);
	gc_assign(t_node->m_parent,t_child);
	return 0;
}
int c_Map6::p_RotateRight6(c_Node9* t_node){
	c_Node9* t_child=t_node->m_left;
	gc_assign(t_node->m_left,t_child->m_right);
	if((t_child->m_right)!=0){
		gc_assign(t_child->m_right->m_parent,t_node);
	}
	gc_assign(t_child->m_parent,t_node->m_parent);
	if((t_node->m_parent)!=0){
		if(t_node==t_node->m_parent->m_right){
			gc_assign(t_node->m_parent->m_right,t_child);
		}else{
			gc_assign(t_node->m_parent->m_left,t_child);
		}
	}else{
		gc_assign(m_root,t_child);
	}
	gc_assign(t_child->m_right,t_node);
	gc_assign(t_node->m_parent,t_child);
	return 0;
}
int c_Map6::p_InsertFixup6(c_Node9* t_node){
	while(((t_node->m_parent)!=0) && t_node->m_parent->m_color==-1 && ((t_node->m_parent->m_parent)!=0)){
		if(t_node->m_parent==t_node->m_parent->m_parent->m_left){
			c_Node9* t_uncle=t_node->m_parent->m_parent->m_right;
			if(((t_uncle)!=0) && t_uncle->m_color==-1){
				t_node->m_parent->m_color=1;
				t_uncle->m_color=1;
				t_uncle->m_parent->m_color=-1;
				t_node=t_uncle->m_parent;
			}else{
				if(t_node==t_node->m_parent->m_right){
					t_node=t_node->m_parent;
					p_RotateLeft7(t_node);
				}
				t_node->m_parent->m_color=1;
				t_node->m_parent->m_parent->m_color=-1;
				p_RotateRight6(t_node->m_parent->m_parent);
			}
		}else{
			c_Node9* t_uncle2=t_node->m_parent->m_parent->m_left;
			if(((t_uncle2)!=0) && t_uncle2->m_color==-1){
				t_node->m_parent->m_color=1;
				t_uncle2->m_color=1;
				t_uncle2->m_parent->m_color=-1;
				t_node=t_uncle2->m_parent;
			}else{
				if(t_node==t_node->m_parent->m_left){
					t_node=t_node->m_parent;
					p_RotateRight6(t_node);
				}
				t_node->m_parent->m_color=1;
				t_node->m_parent->m_parent->m_color=-1;
				p_RotateLeft7(t_node->m_parent->m_parent);
			}
		}
	}
	m_root->m_color=1;
	return 0;
}
bool c_Map6::p_Set14(int t_key,c_DisplayMode* t_value){
	c_Node9* t_node=m_root;
	c_Node9* t_parent=0;
	int t_cmp=0;
	while((t_node)!=0){
		t_parent=t_node;
		t_cmp=p_Compare4(t_key,t_node->m_key);
		if(t_cmp>0){
			t_node=t_node->m_right;
		}else{
			if(t_cmp<0){
				t_node=t_node->m_left;
			}else{
				gc_assign(t_node->m_value,t_value);
				return false;
			}
		}
	}
	t_node=(new c_Node9)->m_new(t_key,t_value,-1,t_parent);
	if((t_parent)!=0){
		if(t_cmp>0){
			gc_assign(t_parent->m_right,t_node);
		}else{
			gc_assign(t_parent->m_left,t_node);
		}
		p_InsertFixup6(t_node);
	}else{
		gc_assign(m_root,t_node);
	}
	return true;
}
bool c_Map6::p_Insert12(int t_key,c_DisplayMode* t_value){
	return p_Set14(t_key,t_value);
}
void c_Map6::mark(){
	Object::mark();
	gc_mark_q(m_root);
}
c_IntMap2::c_IntMap2(){
}
c_IntMap2* c_IntMap2::m_new(){
	c_Map6::m_new();
	return this;
}
int c_IntMap2::p_Compare4(int t_lhs,int t_rhs){
	return t_lhs-t_rhs;
}
void c_IntMap2::mark(){
	c_Map6::mark();
}
c_Stack9::c_Stack9(){
	m_data=Array<c_DisplayMode* >();
	m_length=0;
}
c_Stack9* c_Stack9::m_new(){
	return this;
}
c_Stack9* c_Stack9::m_new2(Array<c_DisplayMode* > t_data){
	gc_assign(this->m_data,t_data.Slice(0));
	this->m_length=t_data.Length();
	return this;
}
void c_Stack9::p_Push25(c_DisplayMode* t_value){
	if(m_length==m_data.Length()){
		gc_assign(m_data,m_data.Resize(m_length*2+10));
	}
	gc_assign(m_data[m_length],t_value);
	m_length+=1;
}
void c_Stack9::p_Push26(Array<c_DisplayMode* > t_values,int t_offset,int t_count){
	for(int t_i=0;t_i<t_count;t_i=t_i+1){
		p_Push25(t_values[t_offset+t_i]);
	}
}
void c_Stack9::p_Push27(Array<c_DisplayMode* > t_values,int t_offset){
	p_Push26(t_values,t_offset,t_values.Length()-t_offset);
}
Array<c_DisplayMode* > c_Stack9::p_ToArray(){
	Array<c_DisplayMode* > t_t=Array<c_DisplayMode* >(m_length);
	for(int t_i=0;t_i<m_length;t_i=t_i+1){
		gc_assign(t_t[t_i],m_data[t_i]);
	}
	return t_t;
}
void c_Stack9::mark(){
	Object::mark();
	gc_mark_q(m_data);
}
c_Node9::c_Node9(){
	m_key=0;
	m_right=0;
	m_left=0;
	m_value=0;
	m_color=0;
	m_parent=0;
}
c_Node9* c_Node9::m_new(int t_key,c_DisplayMode* t_value,int t_color,c_Node9* t_parent){
	this->m_key=t_key;
	gc_assign(this->m_value,t_value);
	this->m_color=t_color;
	gc_assign(this->m_parent,t_parent);
	return this;
}
c_Node9* c_Node9::m_new2(){
	return this;
}
void c_Node9::mark(){
	Object::mark();
	gc_mark_q(m_right);
	gc_mark_q(m_left);
	gc_mark_q(m_value);
	gc_mark_q(m_parent);
}
Array<c_DisplayMode* > bb_app__displayModes;
c_DisplayMode* bb_app__desktopMode;
int bb_app_DeviceWidth(){
	return bb_app__devWidth;
}
int bb_app_DeviceHeight(){
	return bb_app__devHeight;
}
void bb_app_EnumDisplayModes(){
	Array<BBDisplayMode* > t_modes=bb_app__game->GetDisplayModes();
	c_IntMap2* t_mmap=(new c_IntMap2)->m_new();
	c_Stack9* t_mstack=(new c_Stack9)->m_new();
	for(int t_i=0;t_i<t_modes.Length();t_i=t_i+1){
		int t_w=t_modes[t_i]->width;
		int t_h=t_modes[t_i]->height;
		int t_size=t_w<<16|t_h;
		if(t_mmap->p_Contains(t_size)){
		}else{
			c_DisplayMode* t_mode=(new c_DisplayMode)->m_new(t_modes[t_i]->width,t_modes[t_i]->height);
			t_mmap->p_Insert12(t_size,t_mode);
			t_mstack->p_Push25(t_mode);
		}
	}
	gc_assign(bb_app__displayModes,t_mstack->p_ToArray());
	BBDisplayMode* t_mode2=bb_app__game->GetDesktopMode();
	if((t_mode2)!=0){
		gc_assign(bb_app__desktopMode,(new c_DisplayMode)->m_new(t_mode2->width,t_mode2->height));
	}else{
		gc_assign(bb_app__desktopMode,(new c_DisplayMode)->m_new(bb_app_DeviceWidth(),bb_app_DeviceHeight()));
	}
}
int bb_graphics_SetBlend(int t_blend){
	bb_graphics_context->m_blend=t_blend;
	bb_graphics_renderDevice->SetBlend(t_blend);
	return 0;
}
int bb_graphics_SetScissor(Float t_x,Float t_y,Float t_width,Float t_height){
	bb_graphics_context->m_scissor_x=t_x;
	bb_graphics_context->m_scissor_y=t_y;
	bb_graphics_context->m_scissor_width=t_width;
	bb_graphics_context->m_scissor_height=t_height;
	bb_graphics_renderDevice->SetScissor(int(t_x),int(t_y),int(t_width),int(t_height));
	return 0;
}
int bb_graphics_BeginRender(){
	gc_assign(bb_graphics_renderDevice,bb_graphics_device);
	bb_graphics_context->m_matrixSp=0;
	bb_graphics_SetMatrix(FLOAT(1.0),FLOAT(0.0),FLOAT(0.0),FLOAT(1.0),FLOAT(0.0),FLOAT(0.0));
	bb_graphics_SetColor(FLOAT(255.0),FLOAT(255.0),FLOAT(255.0));
	bb_graphics_SetAlpha(FLOAT(1.0));
	bb_graphics_SetBlend(0);
	bb_graphics_SetScissor(FLOAT(0.0),FLOAT(0.0),Float(bb_app_DeviceWidth()),Float(bb_app_DeviceHeight()));
	return 0;
}
int bb_graphics_EndRender(){
	bb_graphics_renderDevice=0;
	return 0;
}
c_BBGameEvent::c_BBGameEvent(){
}
void c_BBGameEvent::mark(){
	Object::mark();
}
void bb_app_EndApp(){
	bbError(String());
}
c_VTransition::c_VTransition(){
	m_duration=FLOAT(.0);
	m_color=(new c_Color)->m_new3(c_Color::m_Black);
	m_active=true;
	m_time=FLOAT(.0);
}
void c_VTransition::p_Duration(Float t_duration){
	this->m_duration=t_duration;
}
Float c_VTransition::p_Duration2(){
	return m_duration;
}
c_VTransition* c_VTransition::m_new(Float t_duration){
	p_Duration(t_duration);
	return this;
}
c_VTransition* c_VTransition::m_new2(){
	bb_functions2_NoDefaultConstructorError(String(L"VTransition",11));
	return this;
}
void c_VTransition::p_SetColor2(c_Color* t_color){
	this->m_color->p_Set8(t_color);
}
void c_VTransition::p_Update4(Float t_dt){
	if(m_active){
		m_time+=t_dt;
		if(m_time>=m_duration){
			m_active=false;
			m_time=m_duration;
		}
	}
}
bool c_VTransition::p_IsActive(){
	return m_active;
}
void c_VTransition::p_Render(){
}
void c_VTransition::mark(){
	Object::mark();
	gc_mark_q(m_color);
}
c_VFadeInLinear::c_VFadeInLinear(){
}
c_VFadeInLinear* c_VFadeInLinear::m_new(){
	c_VTransition::m_new2();
	return this;
}
c_VFadeInLinear* c_VFadeInLinear::m_new2(Float t_duration){
	c_VTransition::m_new(t_duration);
	return this;
}
void c_VFadeInLinear::p_Render(){
	bb_graphics_PushMatrix();
	bb_functions_ResetMatrix();
	bb_functions_ResetBlend();
	Float t_alpha=FLOAT(1.0)-m_time/m_duration;
	bb_graphics_SetAlpha(t_alpha);
	m_color->p_UseWithoutAlpha();
	bb_graphics_DrawRect(FLOAT(0.0),FLOAT(0.0),Float(bb_app2_Vsat->p_ScreenWidth()),Float(bb_app2_Vsat->p_ScreenHeight()));
	bb_graphics_PopMatrix();
}
void c_VFadeInLinear::mark(){
	c_VTransition::mark();
}
c_List4::c_List4(){
	m__head=((new c_HeadNode4)->m_new());
}
c_List4* c_List4::m_new(){
	return this;
}
c_Node10* c_List4::p_AddLast4(c_VShape* t_data){
	return (new c_Node10)->m_new(m__head,m__head->m__pred,t_data);
}
c_List4* c_List4::m_new2(Array<c_VShape* > t_data){
	Array<c_VShape* > t_=t_data;
	int t_2=0;
	while(t_2<t_.Length()){
		c_VShape* t_t=t_[t_2];
		t_2=t_2+1;
		p_AddLast4(t_t);
	}
	return this;
}
c_VShape* c_List4::p_First(){
	return m__head->m__succ->m__data;
}
c_Enumerator12* c_List4::p_ObjectEnumerator(){
	return (new c_Enumerator12)->m_new(this);
}
void c_List4::mark(){
	Object::mark();
	gc_mark_q(m__head);
}
c_Node10::c_Node10(){
	m__succ=0;
	m__pred=0;
	m__data=0;
}
c_Node10* c_Node10::m_new(c_Node10* t_succ,c_Node10* t_pred,c_VShape* t_data){
	gc_assign(m__succ,t_succ);
	gc_assign(m__pred,t_pred);
	gc_assign(m__succ->m__pred,this);
	gc_assign(m__pred->m__succ,this);
	gc_assign(m__data,t_data);
	return this;
}
c_Node10* c_Node10::m_new2(){
	return this;
}
void c_Node10::mark(){
	Object::mark();
	gc_mark_q(m__succ);
	gc_mark_q(m__pred);
	gc_mark_q(m__data);
}
c_HeadNode4::c_HeadNode4(){
}
c_HeadNode4* c_HeadNode4::m_new(){
	c_Node10::m_new2();
	gc_assign(m__succ,(this));
	gc_assign(m__pred,(this));
	return this;
}
void c_HeadNode4::mark(){
	c_Node10::mark();
}
int bb_app__updateRate;
void bb_app_SetUpdateRate(int t_hertz){
	bb_app__updateRate=t_hertz;
	bb_app__game->SetUpdateRate(t_hertz);
}
c_FontCache::c_FontCache(){
}
c_StringMap4* c_FontCache::m_Cache;
c_AngelFont* c_FontCache::m_GetFont(String t_path){
	if(m_Cache->p_Contains3(t_path)){
		return m_Cache->p_Get3(t_path);
	}else{
		c_AngelFont* t_font=(new c_AngelFont)->m_new(String());
		if(!((t_font)!=0)){
			return 0;
		}
		t_font->p_LoadFromXml(t_path);
		m_Cache->p_Set15(t_path,t_font);
		return t_font;
	}
}
void c_FontCache::mark(){
	Object::mark();
}
c_AngelFont::c_AngelFont(){
	m_iniText=String();
	m_chars=Array<c_Char* >(256);
	m_height=0;
	m_heightOffset=9999;
	m_kernPairs=(new c_IntMap4)->m_new();
	m_image=Array<c_Image* >(1);
	m_name=String();
	m_xOffset=0;
	m_yOffset=0;
	m_lineGap=5;
	m_useKerning=true;
}
c_IntMap3* c_AngelFont::m_firstKp;
void c_AngelFont::p_LoadPlain(String t_url){
	m_iniText=bb_app_LoadString(t_url+String(L".fnt",4));
	Array<String > t_lines=m_iniText.Split(String((Char)(10),1));
	Array<String > t_attribs=Array<String >();
	int t_pageCount=0;
	Array<String > t_=t_lines;
	int t_2=0;
	while(t_2<t_.Length()){
		String t_line=t_[t_2];
		t_2=t_2+1;
		t_line=t_line.Trim();
		if(t_line.StartsWith(String(L"info",4))){
			continue;
		}else{
			if(t_line.StartsWith(String(L"common",6))){
				continue;
			}else{
				if(t_line.StartsWith(String(L"chars",5))){
					continue;
				}else{
					if(t_line.StartsWith(String(L"char ",5))){
						int t_id=0;
						int t_x=0;
						int t_y=0;
						int t_w=0;
						int t_h=0;
						int t_xOffset=0;
						int t_yOffset=0;
						int t_xAdvance=0;
						int t_page=0;
						t_attribs=t_line.Split(String(L" ",1));
						for(int t_i=0;t_i<t_attribs.Length();t_i=t_i+1){
							t_attribs[t_i].Trim();
							if(t_attribs[t_i]==String()){
								continue;
							}else{
								if(t_attribs[t_i].StartsWith(String(L"id=",3))){
									t_id=(t_attribs[t_i].Slice(t_attribs[t_i].FindLast(String(L"=",1))+1)).ToInt();
								}else{
									if(t_attribs[t_i].StartsWith(String(L"x=",2))){
										t_x=(t_attribs[t_i].Slice(t_attribs[t_i].FindLast(String(L"=",1))+1)).ToInt();
									}else{
										if(t_attribs[t_i].StartsWith(String(L"y=",2))){
											t_y=(t_attribs[t_i].Slice(t_attribs[t_i].FindLast(String(L"=",1))+1)).ToInt();
										}else{
											if(t_attribs[t_i].StartsWith(String(L"width=",6))){
												t_w=(t_attribs[t_i].Slice(t_attribs[t_i].FindLast(String(L"=",1))+1)).ToInt();
											}else{
												if(t_attribs[t_i].StartsWith(String(L"height=",7))){
													t_h=(t_attribs[t_i].Slice(t_attribs[t_i].FindLast(String(L"=",1))+1)).ToInt();
												}else{
													if(t_attribs[t_i].StartsWith(String(L"xoffset=",8))){
														t_xOffset=(t_attribs[t_i].Slice(t_attribs[t_i].FindLast(String(L"=",1))+1)).ToInt();
													}else{
														if(t_attribs[t_i].StartsWith(String(L"yoffset=",8))){
															t_yOffset=(t_attribs[t_i].Slice(t_attribs[t_i].FindLast(String(L"=",1))+1)).ToInt();
														}else{
															if(t_attribs[t_i].StartsWith(String(L"xadvance=",9))){
																t_xAdvance=(t_attribs[t_i].Slice(t_attribs[t_i].FindLast(String(L"=",1))+1)).ToInt();
															}else{
																if(t_attribs[t_i].StartsWith(String(L"page=",5))){
																	t_page=(t_attribs[t_i].Slice(t_attribs[t_i].FindLast(String(L"=",1))+1)).ToInt();
																	if(t_pageCount<t_page){
																		t_pageCount=t_page;
																	}
																}
															}
														}
													}
												}
											}
										}
									}
								}
							}
						}
						gc_assign(m_chars[t_id],(new c_Char)->m_new(t_x,t_y,t_w,t_h,t_xOffset,t_yOffset,t_xAdvance,t_page));
						c_Char* t_ch=m_chars[t_id];
						if(t_ch->m_height>this->m_height){
							this->m_height=t_ch->m_height;
						}
						if(t_ch->m_yOffset<this->m_heightOffset){
							this->m_heightOffset=t_ch->m_yOffset;
						}
					}else{
						if(t_line.StartsWith(String(L"kernings",8))){
							continue;
						}else{
							if(t_line.StartsWith(String(L"kerning ",8))){
								int t_first=0;
								int t_second=0;
								int t_amount=0;
								t_attribs=t_line.Split(String(L" ",1));
								for(int t_i2=0;t_i2<t_attribs.Length();t_i2=t_i2+1){
									t_attribs[t_i2].Trim();
									if(t_attribs[t_i2]==String()){
										continue;
									}else{
										if(t_attribs[t_i2].StartsWith(String(L"first=",6))){
											t_first=(t_attribs[t_i2].Slice(t_attribs[t_i2].FindLast(String(L"=",1))+1)).ToInt();
										}else{
											if(t_attribs[t_i2].StartsWith(String(L"second=",7))){
												t_second=(t_attribs[t_i2].Slice(t_attribs[t_i2].FindLast(String(L"=",1))+1)).ToInt();
											}else{
												if(t_attribs[t_i2].StartsWith(String(L"amount=",7))){
													t_amount=(t_attribs[t_i2].Slice(t_attribs[t_i2].FindLast(String(L"=",1))+1)).ToInt();
												}
											}
										}
									}
								}
								gc_assign(m_firstKp,m_kernPairs->p_Get(t_first));
								if(m_firstKp==0){
									m_kernPairs->p_Add9(t_first,(new c_IntMap3)->m_new());
									gc_assign(m_firstKp,m_kernPairs->p_Get(t_first));
								}
								m_firstKp->p_Add8(t_second,(new c_KernPair)->m_new(t_first,t_second,t_amount));
							}
						}
					}
				}
			}
		}
	}
	for(int t_page2=0;t_page2<=t_pageCount;t_page2=t_page2+1){
		if(m_image.Length()<t_page2+1){
			gc_assign(m_image,m_image.Resize(t_page2+1));
		}
		gc_assign(m_image[t_page2],bb_graphics_LoadImage(t_url+String(L"_",1)+String(t_page2)+String(L".png",4),1,c_Image::m_DefaultFlags));
	}
}
c_StringMap4* c_AngelFont::m__list;
c_AngelFont* c_AngelFont::m_new(String t_url){
	if(t_url!=String()){
		this->p_LoadPlain(t_url);
		this->m_name=t_url;
		m__list->p_Insert13(t_url,this);
	}
	return this;
}
void c_AngelFont::p_LoadFromXml(String t_url){
	m_iniText=bb_app_LoadString(t_url+String(L".fnt",4));
	c_XMLError* t_error=(new c_XMLError)->m_new();
	int t_pageCount=0;
	c_XMLDoc* t_config=bb_xml_ParseXML(m_iniText,t_error,1);
	if(t_config==0 && t_error->m_error){
		bbPrint(t_error->p_ToString());
	}else{
		c_List5* t_nodes=t_config->p_GetChildrenAtPath(String(L"chars/char",10));
		c_Enumerator10* t_=t_nodes->p_ObjectEnumerator();
		while(t_->p_HasNext()){
			c_XMLNode* t_node=t_->p_NextObject();
			int t_id=(t_node->p_GetAttribute(String(L"id",2))).ToInt();
			int t_page=(t_node->p_GetAttribute(String(L"page",4))).ToInt();
			if(t_pageCount<t_page){
				t_pageCount=t_page;
			}
			gc_assign(m_chars[t_id],(new c_Char)->m_new((t_node->p_GetAttribute(String(L"x",1))).ToInt(),(t_node->p_GetAttribute(String(L"y",1))).ToInt(),(t_node->p_GetAttribute(String(L"width",5))).ToInt(),(t_node->p_GetAttribute(String(L"height",6))).ToInt(),(t_node->p_GetAttribute(String(L"xoffset",7))).ToInt(),(t_node->p_GetAttribute(String(L"yoffset",7))).ToInt(),(t_node->p_GetAttribute(String(L"xadvance",8))).ToInt(),t_page));
			c_Char* t_ch=m_chars[t_id];
			if(t_ch->m_height>this->m_height){
				this->m_height=t_ch->m_height;
			}
			if(t_ch->m_yOffset<this->m_heightOffset){
				this->m_heightOffset=t_ch->m_yOffset;
			}
		}
		t_nodes=t_config->p_GetChildrenAtPath(String(L"kernings/kerning",16));
		c_Enumerator10* t_2=t_nodes->p_ObjectEnumerator();
		while(t_2->p_HasNext()){
			c_XMLNode* t_node2=t_2->p_NextObject();
			int t_first=(t_node2->p_GetAttribute(String(L"first",5))).ToInt();
			gc_assign(m_firstKp,m_kernPairs->p_Get(t_first));
			if(m_firstKp==0){
				m_kernPairs->p_Add9(t_first,(new c_IntMap3)->m_new());
				gc_assign(m_firstKp,m_kernPairs->p_Get(t_first));
			}
			int t_second=(t_node2->p_GetAttribute(String(L"second",6))).ToInt();
			m_firstKp->p_Add8(t_second,(new c_KernPair)->m_new(t_first,t_second,(t_node2->p_GetAttribute(String(L"amount",6))).ToInt()));
		}
		if(t_pageCount==0){
			gc_assign(m_image[0],bb_graphics_LoadImage(t_url+String(L".png",4),1,c_Image::m_DefaultFlags));
			if(m_image[0]==0){
				gc_assign(m_image[0],bb_graphics_LoadImage(t_url+String(L"_0.png",6),1,c_Image::m_DefaultFlags));
			}
		}else{
			for(int t_page2=0;t_page2<=t_pageCount;t_page2=t_page2+1){
				if(m_image.Length()<t_page2+1){
					gc_assign(m_image,m_image.Resize(t_page2+1));
				}
				gc_assign(m_image[t_page2],bb_graphics_LoadImage(t_url+String(L"_",1)+String(t_page2)+String(L".png",4),1,c_Image::m_DefaultFlags));
			}
		}
	}
}
c_KernPair* c_AngelFont::m_secondKp;
void c_AngelFont::p_DrawText(String t_txt,int t_x,int t_y){
	int t_prevChar=0;
	m_xOffset=0;
	m_yOffset=0;
	for(int t_i=0;t_i<t_txt.Length();t_i=t_i+1){
		int t_asc=(int)t_txt[t_i];
		c_Char* t_ac=m_chars[t_asc];
		int t_thisChar=t_asc;
		if(t_thisChar==10 || t_thisChar==13){
			m_xOffset=0;
			m_yOffset+=m_lineGap+m_height;
			continue;
		}
		if(t_ac!=0){
			if(m_useKerning){
				gc_assign(m_firstKp,m_kernPairs->p_Get(t_prevChar));
				if(m_firstKp!=0){
					gc_assign(m_secondKp,m_firstKp->p_Get(t_thisChar));
					if(m_secondKp!=0){
						m_xOffset+=m_secondKp->m_amount;
					}
				}
			}
			t_ac->p_Draw2(m_image[t_ac->m_page],int((Float)floor(Float(t_x+m_xOffset)+FLOAT(0.5))),int((Float)floor(Float(t_y+m_yOffset)+FLOAT(0.5))));
			m_xOffset+=t_ac->m_xAdvance;
			t_prevChar=t_thisChar;
		}
	}
}
int c_AngelFont::p_TextWidth(String t_txt){
	int t_prevChar=0;
	int t_width=0;
	int t_lineRecord=0;
	for(int t_i=0;t_i<t_txt.Length();t_i=t_i+1){
		int t_asc=(int)t_txt[t_i];
		c_Char* t_ac=m_chars[t_asc];
		int t_thisChar=t_asc;
		if(t_thisChar==10 || t_thisChar==13){
			if(t_width>t_lineRecord){
				t_lineRecord=t_width;
			}
			t_width=0;
			continue;
		}
		if(t_ac!=0){
			if(m_useKerning){
				c_IntMap3* t_firstKp=m_kernPairs->p_Get(t_prevChar);
				if(t_firstKp!=0){
					c_KernPair* t_secondKp=t_firstKp->p_Get(t_thisChar);
					if(t_secondKp!=0){
						m_xOffset+=t_secondKp->m_amount;
					}
				}
			}
			t_width+=t_ac->m_xAdvance;
			t_prevChar=t_thisChar;
		}
	}
	if((t_lineRecord)!=0){
		return t_lineRecord;
	}
	return t_width;
}
int c_AngelFont::p_TextHeight(String t_txt){
	int t_h=0;
	bool t_hasNewline=false;
	for(int t_i=0;t_i<t_txt.Length();t_i=t_i+1){
		int t_asc=(int)t_txt[t_i];
		c_Char* t_ac=m_chars[t_asc];
		if(t_asc==10 || t_asc==13){
			t_h+=m_lineGap+m_height;
			t_hasNewline=true;
			continue;
		}
		if(t_ac->m_height+t_ac->m_yOffset>t_h){
			t_h=t_ac->m_height+t_ac->m_yOffset;
		}
	}
	if(t_hasNewline){
		t_h-=m_lineGap;
	}
	return t_h;
}
void c_AngelFont::p_DrawText2(String t_txt,int t_x,int t_y,int t_horizontalAlign,int t_verticalAlign){
	m_xOffset=0;
	int t_1=t_horizontalAlign;
	if(t_1==1){
		t_x=t_x-p_TextWidth(t_txt)/2;
	}else{
		if(t_1==2){
			t_x=t_x-p_TextWidth(t_txt);
		}else{
			if(t_1==0){
			}
		}
	}
	int t_2=t_verticalAlign;
	if(t_2==1){
		t_y=t_y-p_TextHeight(t_txt)/2;
	}else{
		if(t_2==3){
		}
	}
	p_DrawText(t_txt,t_x,t_y);
}
void c_AngelFont::mark(){
	Object::mark();
	gc_mark_q(m_chars);
	gc_mark_q(m_kernPairs);
	gc_mark_q(m_image);
}
c_Map7::c_Map7(){
	m_root=0;
}
c_Map7* c_Map7::m_new(){
	return this;
}
c_Node11* c_Map7::p_FindNode3(String t_key){
	c_Node11* t_node=m_root;
	while((t_node)!=0){
		int t_cmp=p_Compare6(t_key,t_node->m_key);
		if(t_cmp>0){
			t_node=t_node->m_right;
		}else{
			if(t_cmp<0){
				t_node=t_node->m_left;
			}else{
				return t_node;
			}
		}
	}
	return t_node;
}
bool c_Map7::p_Contains3(String t_key){
	return p_FindNode3(t_key)!=0;
}
c_AngelFont* c_Map7::p_Get3(String t_key){
	c_Node11* t_node=p_FindNode3(t_key);
	if((t_node)!=0){
		return t_node->m_value;
	}
	return 0;
}
int c_Map7::p_RotateLeft8(c_Node11* t_node){
	c_Node11* t_child=t_node->m_right;
	gc_assign(t_node->m_right,t_child->m_left);
	if((t_child->m_left)!=0){
		gc_assign(t_child->m_left->m_parent,t_node);
	}
	gc_assign(t_child->m_parent,t_node->m_parent);
	if((t_node->m_parent)!=0){
		if(t_node==t_node->m_parent->m_left){
			gc_assign(t_node->m_parent->m_left,t_child);
		}else{
			gc_assign(t_node->m_parent->m_right,t_child);
		}
	}else{
		gc_assign(m_root,t_child);
	}
	gc_assign(t_child->m_left,t_node);
	gc_assign(t_node->m_parent,t_child);
	return 0;
}
int c_Map7::p_RotateRight7(c_Node11* t_node){
	c_Node11* t_child=t_node->m_left;
	gc_assign(t_node->m_left,t_child->m_right);
	if((t_child->m_right)!=0){
		gc_assign(t_child->m_right->m_parent,t_node);
	}
	gc_assign(t_child->m_parent,t_node->m_parent);
	if((t_node->m_parent)!=0){
		if(t_node==t_node->m_parent->m_right){
			gc_assign(t_node->m_parent->m_right,t_child);
		}else{
			gc_assign(t_node->m_parent->m_left,t_child);
		}
	}else{
		gc_assign(m_root,t_child);
	}
	gc_assign(t_child->m_right,t_node);
	gc_assign(t_node->m_parent,t_child);
	return 0;
}
int c_Map7::p_InsertFixup7(c_Node11* t_node){
	while(((t_node->m_parent)!=0) && t_node->m_parent->m_color==-1 && ((t_node->m_parent->m_parent)!=0)){
		if(t_node->m_parent==t_node->m_parent->m_parent->m_left){
			c_Node11* t_uncle=t_node->m_parent->m_parent->m_right;
			if(((t_uncle)!=0) && t_uncle->m_color==-1){
				t_node->m_parent->m_color=1;
				t_uncle->m_color=1;
				t_uncle->m_parent->m_color=-1;
				t_node=t_uncle->m_parent;
			}else{
				if(t_node==t_node->m_parent->m_right){
					t_node=t_node->m_parent;
					p_RotateLeft8(t_node);
				}
				t_node->m_parent->m_color=1;
				t_node->m_parent->m_parent->m_color=-1;
				p_RotateRight7(t_node->m_parent->m_parent);
			}
		}else{
			c_Node11* t_uncle2=t_node->m_parent->m_parent->m_left;
			if(((t_uncle2)!=0) && t_uncle2->m_color==-1){
				t_node->m_parent->m_color=1;
				t_uncle2->m_color=1;
				t_uncle2->m_parent->m_color=-1;
				t_node=t_uncle2->m_parent;
			}else{
				if(t_node==t_node->m_parent->m_left){
					t_node=t_node->m_parent;
					p_RotateRight7(t_node);
				}
				t_node->m_parent->m_color=1;
				t_node->m_parent->m_parent->m_color=-1;
				p_RotateLeft8(t_node->m_parent->m_parent);
			}
		}
	}
	m_root->m_color=1;
	return 0;
}
bool c_Map7::p_Set15(String t_key,c_AngelFont* t_value){
	c_Node11* t_node=m_root;
	c_Node11* t_parent=0;
	int t_cmp=0;
	while((t_node)!=0){
		t_parent=t_node;
		t_cmp=p_Compare6(t_key,t_node->m_key);
		if(t_cmp>0){
			t_node=t_node->m_right;
		}else{
			if(t_cmp<0){
				t_node=t_node->m_left;
			}else{
				gc_assign(t_node->m_value,t_value);
				return false;
			}
		}
	}
	t_node=(new c_Node11)->m_new(t_key,t_value,-1,t_parent);
	if((t_parent)!=0){
		if(t_cmp>0){
			gc_assign(t_parent->m_right,t_node);
		}else{
			gc_assign(t_parent->m_left,t_node);
		}
		p_InsertFixup7(t_node);
	}else{
		gc_assign(m_root,t_node);
	}
	return true;
}
bool c_Map7::p_Insert13(String t_key,c_AngelFont* t_value){
	return p_Set15(t_key,t_value);
}
void c_Map7::mark(){
	Object::mark();
	gc_mark_q(m_root);
}
c_StringMap4::c_StringMap4(){
}
c_StringMap4* c_StringMap4::m_new(){
	c_Map7::m_new();
	return this;
}
int c_StringMap4::p_Compare6(String t_lhs,String t_rhs){
	return t_lhs.Compare(t_rhs);
}
void c_StringMap4::mark(){
	c_Map7::mark();
}
c_Node11::c_Node11(){
	m_key=String();
	m_right=0;
	m_left=0;
	m_value=0;
	m_color=0;
	m_parent=0;
}
c_Node11* c_Node11::m_new(String t_key,c_AngelFont* t_value,int t_color,c_Node11* t_parent){
	this->m_key=t_key;
	gc_assign(this->m_value,t_value);
	this->m_color=t_color;
	gc_assign(this->m_parent,t_parent);
	return this;
}
c_Node11* c_Node11::m_new2(){
	return this;
}
void c_Node11::mark(){
	Object::mark();
	gc_mark_q(m_right);
	gc_mark_q(m_left);
	gc_mark_q(m_value);
	gc_mark_q(m_parent);
}
String bb_app_LoadString(String t_path){
	return bb_app__game->LoadString(bb_data_FixDataPath(t_path));
}
c_Char::c_Char(){
	m_x=0;
	m_y=0;
	m_width=0;
	m_height=0;
	m_xOffset=0;
	m_yOffset=0;
	m_xAdvance=0;
	m_page=0;
}
c_Char* c_Char::m_new(int t_x,int t_y,int t_w,int t_h,int t_xoff,int t_yoff,int t_xadv,int t_page){
	this->m_x=t_x;
	this->m_y=t_y;
	this->m_width=t_w;
	this->m_height=t_h;
	this->m_xOffset=t_xoff;
	this->m_yOffset=t_yoff;
	this->m_xAdvance=t_xadv;
	this->m_page=t_page;
	return this;
}
c_Char* c_Char::m_new2(){
	return this;
}
void c_Char::p_Draw2(c_Image* t_fontImage,int t_linex,int t_liney){
	bb_graphics_DrawImageRect(t_fontImage,Float(t_linex+m_xOffset),Float(t_liney+m_yOffset),m_x,m_y,m_width,m_height,0);
}
void c_Char::mark(){
	Object::mark();
}
c_KernPair::c_KernPair(){
	m_first=String();
	m_second=String();
	m_amount=0;
}
c_KernPair* c_KernPair::m_new(int t_first,int t_second,int t_amount){
	this->m_first=String(t_first);
	this->m_second=String(t_second);
	this->m_amount=t_amount;
	return this;
}
c_KernPair* c_KernPair::m_new2(){
	return this;
}
void c_KernPair::mark(){
	Object::mark();
}
c_Map8::c_Map8(){
	m_root=0;
}
c_Map8* c_Map8::m_new(){
	return this;
}
int c_Map8::p_RotateLeft9(c_Node13* t_node){
	c_Node13* t_child=t_node->m_right;
	gc_assign(t_node->m_right,t_child->m_left);
	if((t_child->m_left)!=0){
		gc_assign(t_child->m_left->m_parent,t_node);
	}
	gc_assign(t_child->m_parent,t_node->m_parent);
	if((t_node->m_parent)!=0){
		if(t_node==t_node->m_parent->m_left){
			gc_assign(t_node->m_parent->m_left,t_child);
		}else{
			gc_assign(t_node->m_parent->m_right,t_child);
		}
	}else{
		gc_assign(m_root,t_child);
	}
	gc_assign(t_child->m_left,t_node);
	gc_assign(t_node->m_parent,t_child);
	return 0;
}
int c_Map8::p_RotateRight8(c_Node13* t_node){
	c_Node13* t_child=t_node->m_left;
	gc_assign(t_node->m_left,t_child->m_right);
	if((t_child->m_right)!=0){
		gc_assign(t_child->m_right->m_parent,t_node);
	}
	gc_assign(t_child->m_parent,t_node->m_parent);
	if((t_node->m_parent)!=0){
		if(t_node==t_node->m_parent->m_right){
			gc_assign(t_node->m_parent->m_right,t_child);
		}else{
			gc_assign(t_node->m_parent->m_left,t_child);
		}
	}else{
		gc_assign(m_root,t_child);
	}
	gc_assign(t_child->m_right,t_node);
	gc_assign(t_node->m_parent,t_child);
	return 0;
}
int c_Map8::p_InsertFixup8(c_Node13* t_node){
	while(((t_node->m_parent)!=0) && t_node->m_parent->m_color==-1 && ((t_node->m_parent->m_parent)!=0)){
		if(t_node->m_parent==t_node->m_parent->m_parent->m_left){
			c_Node13* t_uncle=t_node->m_parent->m_parent->m_right;
			if(((t_uncle)!=0) && t_uncle->m_color==-1){
				t_node->m_parent->m_color=1;
				t_uncle->m_color=1;
				t_uncle->m_parent->m_color=-1;
				t_node=t_uncle->m_parent;
			}else{
				if(t_node==t_node->m_parent->m_right){
					t_node=t_node->m_parent;
					p_RotateLeft9(t_node);
				}
				t_node->m_parent->m_color=1;
				t_node->m_parent->m_parent->m_color=-1;
				p_RotateRight8(t_node->m_parent->m_parent);
			}
		}else{
			c_Node13* t_uncle2=t_node->m_parent->m_parent->m_left;
			if(((t_uncle2)!=0) && t_uncle2->m_color==-1){
				t_node->m_parent->m_color=1;
				t_uncle2->m_color=1;
				t_uncle2->m_parent->m_color=-1;
				t_node=t_uncle2->m_parent;
			}else{
				if(t_node==t_node->m_parent->m_left){
					t_node=t_node->m_parent;
					p_RotateRight8(t_node);
				}
				t_node->m_parent->m_color=1;
				t_node->m_parent->m_parent->m_color=-1;
				p_RotateLeft9(t_node->m_parent->m_parent);
			}
		}
	}
	m_root->m_color=1;
	return 0;
}
bool c_Map8::p_Add8(int t_key,c_KernPair* t_value){
	c_Node13* t_node=m_root;
	c_Node13* t_parent=0;
	int t_cmp=0;
	while((t_node)!=0){
		t_parent=t_node;
		t_cmp=p_Compare4(t_key,t_node->m_key);
		if(t_cmp>0){
			t_node=t_node->m_right;
		}else{
			if(t_cmp<0){
				t_node=t_node->m_left;
			}else{
				return false;
			}
		}
	}
	t_node=(new c_Node13)->m_new(t_key,t_value,-1,t_parent);
	if((t_parent)!=0){
		if(t_cmp>0){
			gc_assign(t_parent->m_right,t_node);
		}else{
			gc_assign(t_parent->m_left,t_node);
		}
		p_InsertFixup8(t_node);
	}else{
		gc_assign(m_root,t_node);
	}
	return true;
}
c_Node13* c_Map8::p_FindNode(int t_key){
	c_Node13* t_node=m_root;
	while((t_node)!=0){
		int t_cmp=p_Compare4(t_key,t_node->m_key);
		if(t_cmp>0){
			t_node=t_node->m_right;
		}else{
			if(t_cmp<0){
				t_node=t_node->m_left;
			}else{
				return t_node;
			}
		}
	}
	return t_node;
}
c_KernPair* c_Map8::p_Get(int t_key){
	c_Node13* t_node=p_FindNode(t_key);
	if((t_node)!=0){
		return t_node->m_value;
	}
	return 0;
}
void c_Map8::mark(){
	Object::mark();
	gc_mark_q(m_root);
}
c_IntMap3::c_IntMap3(){
}
c_IntMap3* c_IntMap3::m_new(){
	c_Map8::m_new();
	return this;
}
int c_IntMap3::p_Compare4(int t_lhs,int t_rhs){
	return t_lhs-t_rhs;
}
void c_IntMap3::mark(){
	c_Map8::mark();
}
c_Map9::c_Map9(){
	m_root=0;
}
c_Map9* c_Map9::m_new(){
	return this;
}
c_Node12* c_Map9::p_FindNode(int t_key){
	c_Node12* t_node=m_root;
	while((t_node)!=0){
		int t_cmp=p_Compare4(t_key,t_node->m_key);
		if(t_cmp>0){
			t_node=t_node->m_right;
		}else{
			if(t_cmp<0){
				t_node=t_node->m_left;
			}else{
				return t_node;
			}
		}
	}
	return t_node;
}
c_IntMap3* c_Map9::p_Get(int t_key){
	c_Node12* t_node=p_FindNode(t_key);
	if((t_node)!=0){
		return t_node->m_value;
	}
	return 0;
}
int c_Map9::p_RotateLeft10(c_Node12* t_node){
	c_Node12* t_child=t_node->m_right;
	gc_assign(t_node->m_right,t_child->m_left);
	if((t_child->m_left)!=0){
		gc_assign(t_child->m_left->m_parent,t_node);
	}
	gc_assign(t_child->m_parent,t_node->m_parent);
	if((t_node->m_parent)!=0){
		if(t_node==t_node->m_parent->m_left){
			gc_assign(t_node->m_parent->m_left,t_child);
		}else{
			gc_assign(t_node->m_parent->m_right,t_child);
		}
	}else{
		gc_assign(m_root,t_child);
	}
	gc_assign(t_child->m_left,t_node);
	gc_assign(t_node->m_parent,t_child);
	return 0;
}
int c_Map9::p_RotateRight9(c_Node12* t_node){
	c_Node12* t_child=t_node->m_left;
	gc_assign(t_node->m_left,t_child->m_right);
	if((t_child->m_right)!=0){
		gc_assign(t_child->m_right->m_parent,t_node);
	}
	gc_assign(t_child->m_parent,t_node->m_parent);
	if((t_node->m_parent)!=0){
		if(t_node==t_node->m_parent->m_right){
			gc_assign(t_node->m_parent->m_right,t_child);
		}else{
			gc_assign(t_node->m_parent->m_left,t_child);
		}
	}else{
		gc_assign(m_root,t_child);
	}
	gc_assign(t_child->m_right,t_node);
	gc_assign(t_node->m_parent,t_child);
	return 0;
}
int c_Map9::p_InsertFixup9(c_Node12* t_node){
	while(((t_node->m_parent)!=0) && t_node->m_parent->m_color==-1 && ((t_node->m_parent->m_parent)!=0)){
		if(t_node->m_parent==t_node->m_parent->m_parent->m_left){
			c_Node12* t_uncle=t_node->m_parent->m_parent->m_right;
			if(((t_uncle)!=0) && t_uncle->m_color==-1){
				t_node->m_parent->m_color=1;
				t_uncle->m_color=1;
				t_uncle->m_parent->m_color=-1;
				t_node=t_uncle->m_parent;
			}else{
				if(t_node==t_node->m_parent->m_right){
					t_node=t_node->m_parent;
					p_RotateLeft10(t_node);
				}
				t_node->m_parent->m_color=1;
				t_node->m_parent->m_parent->m_color=-1;
				p_RotateRight9(t_node->m_parent->m_parent);
			}
		}else{
			c_Node12* t_uncle2=t_node->m_parent->m_parent->m_left;
			if(((t_uncle2)!=0) && t_uncle2->m_color==-1){
				t_node->m_parent->m_color=1;
				t_uncle2->m_color=1;
				t_uncle2->m_parent->m_color=-1;
				t_node=t_uncle2->m_parent;
			}else{
				if(t_node==t_node->m_parent->m_left){
					t_node=t_node->m_parent;
					p_RotateRight9(t_node);
				}
				t_node->m_parent->m_color=1;
				t_node->m_parent->m_parent->m_color=-1;
				p_RotateLeft10(t_node->m_parent->m_parent);
			}
		}
	}
	m_root->m_color=1;
	return 0;
}
bool c_Map9::p_Add9(int t_key,c_IntMap3* t_value){
	c_Node12* t_node=m_root;
	c_Node12* t_parent=0;
	int t_cmp=0;
	while((t_node)!=0){
		t_parent=t_node;
		t_cmp=p_Compare4(t_key,t_node->m_key);
		if(t_cmp>0){
			t_node=t_node->m_right;
		}else{
			if(t_cmp<0){
				t_node=t_node->m_left;
			}else{
				return false;
			}
		}
	}
	t_node=(new c_Node12)->m_new(t_key,t_value,-1,t_parent);
	if((t_parent)!=0){
		if(t_cmp>0){
			gc_assign(t_parent->m_right,t_node);
		}else{
			gc_assign(t_parent->m_left,t_node);
		}
		p_InsertFixup9(t_node);
	}else{
		gc_assign(m_root,t_node);
	}
	return true;
}
void c_Map9::mark(){
	Object::mark();
	gc_mark_q(m_root);
}
c_IntMap4::c_IntMap4(){
}
c_IntMap4* c_IntMap4::m_new(){
	c_Map9::m_new();
	return this;
}
int c_IntMap4::p_Compare4(int t_lhs,int t_rhs){
	return t_lhs-t_rhs;
}
void c_IntMap4::mark(){
	c_Map9::mark();
}
c_Node12::c_Node12(){
	m_key=0;
	m_right=0;
	m_left=0;
	m_value=0;
	m_color=0;
	m_parent=0;
}
c_Node12* c_Node12::m_new(int t_key,c_IntMap3* t_value,int t_color,c_Node12* t_parent){
	this->m_key=t_key;
	gc_assign(this->m_value,t_value);
	this->m_color=t_color;
	gc_assign(this->m_parent,t_parent);
	return this;
}
c_Node12* c_Node12::m_new2(){
	return this;
}
void c_Node12::mark(){
	Object::mark();
	gc_mark_q(m_right);
	gc_mark_q(m_left);
	gc_mark_q(m_value);
	gc_mark_q(m_parent);
}
c_Node13::c_Node13(){
	m_key=0;
	m_right=0;
	m_left=0;
	m_value=0;
	m_color=0;
	m_parent=0;
}
c_Node13* c_Node13::m_new(int t_key,c_KernPair* t_value,int t_color,c_Node13* t_parent){
	this->m_key=t_key;
	gc_assign(this->m_value,t_value);
	this->m_color=t_color;
	gc_assign(this->m_parent,t_parent);
	return this;
}
c_Node13* c_Node13::m_new2(){
	return this;
}
void c_Node13::mark(){
	Object::mark();
	gc_mark_q(m_right);
	gc_mark_q(m_left);
	gc_mark_q(m_value);
	gc_mark_q(m_parent);
}
c_XMLError::c_XMLError(){
	m_error=false;
	m_message=String();
	m_line=0;
	m_column=0;
	m_offset=0;
}
c_XMLError* c_XMLError::m_new(){
	return this;
}
void c_XMLError::p_Reset(){
	m_error=false;
	m_message=String();
	m_line=-1;
	m_column=-1;
	m_offset=-1;
}
void c_XMLError::p_Set16(String t_message,int t_line,int t_column,int t_offset){
	m_error=true;
	this->m_message=t_message;
	this->m_line=t_line;
	this->m_column=t_column;
	this->m_offset=t_offset;
}
String c_XMLError::p_ToString(){
	if(m_error==false){
		return String();
	}
	c_XMLStringBuffer* t_buffer=(new c_XMLStringBuffer)->m_new(256);
	t_buffer->p_Add11(String(L"XMLError: ",10));
	if((m_message.Length())!=0){
		t_buffer->p_Add11(m_message);
	}else{
		t_buffer->p_Add11(String(L"unknown error",13));
	}
	t_buffer->p_Add11(String(L" [line:",7));
	if(m_line>-1){
		t_buffer->p_Add11(String(m_line));
	}else{
		t_buffer->p_Add11(String(L"??",2));
	}
	t_buffer->p_Add11(String(L"  column:",9));
	if(m_column>-1){
		t_buffer->p_Add11(String(m_column));
	}else{
		t_buffer->p_Add11(String(L"??",2));
	}
	t_buffer->p_Add11(String(L"  offset:",9));
	if(m_offset>-1){
		t_buffer->p_Add11(String(m_offset)+String(L"]",1));
	}else{
		t_buffer->p_Add11(String(L"??]",3));
	}
	return t_buffer->p_value();
}
void c_XMLError::mark(){
	Object::mark();
}
c_XMLNode::c_XMLNode(){
	m_value=String();
	m_name=String();
	m_valid=false;
	m_doc=0;
	m_path=String();
	m_pathList=0;
	m_pathListNode=0;
	m_parent=0;
	m_line=0;
	m_column=0;
	m_offset=0;
	m_attributes=(new c_StringMap6)->m_new();
	m_lastChild=0;
	m_nextSibling=0;
	m_previousSibling=0;
	m_firstChild=0;
	m_children=(new c_List5)->m_new();
}
c_XMLNode* c_XMLNode::m_new(String t_name,bool t_valid){
	if((t_name.Length())!=0){
		this->m_name=t_name.ToLower();
	}
	this->m_valid=t_valid;
	return this;
}
c_XMLNode* c_XMLNode::m_new2(){
	return this;
}
void c_XMLNode::p_SetAttribute3(String t_id){
	if(m_valid==false){
		return;
	}
	t_id=t_id.ToLower();
	c_XMLAttribute* t_attribute=m_attributes->p_Get3(t_id);
	if(t_attribute==0){
		m_attributes->p_Insert15(t_id,(new c_XMLAttribute)->m_new(t_id,String()));
	}else{
		t_attribute->m_value=String();
	}
}
void c_XMLNode::p_SetAttribute2(String t_id,bool t_value){
	if(m_valid==false){
		return;
	}
	t_id=t_id.ToLower();
	c_XMLAttribute* t_attribute=m_attributes->p_Get3(t_id);
	if(t_attribute==0){
		m_attributes->p_Insert15(t_id,(new c_XMLAttribute)->m_new(t_id,String((t_value)?1:0)));
	}else{
		t_attribute->m_value=String((t_value)?1:0);
	}
}
void c_XMLNode::p_SetAttribute4(String t_id,int t_value){
	if(m_valid==false){
		return;
	}
	t_id=t_id.ToLower();
	c_XMLAttribute* t_attribute=m_attributes->p_Get3(t_id);
	if(t_attribute==0){
		m_attributes->p_Insert15(t_id,(new c_XMLAttribute)->m_new(t_id,String(t_value)));
	}else{
		t_attribute->m_value=String(t_value);
	}
}
void c_XMLNode::p_SetAttribute5(String t_id,Float t_value){
	if(m_valid==false){
		return;
	}
	t_id=t_id.ToLower();
	c_XMLAttribute* t_attribute=m_attributes->p_Get3(t_id);
	if(t_attribute==0){
		m_attributes->p_Insert15(t_id,(new c_XMLAttribute)->m_new(t_id,String(t_value)));
	}else{
		t_attribute->m_value=String(t_value);
	}
}
void c_XMLNode::p_SetAttribute(String t_id,String t_value){
	if(m_valid==false){
		return;
	}
	t_id=t_id.ToLower();
	c_XMLAttribute* t_attribute=m_attributes->p_Get3(t_id);
	if(t_attribute==0){
		m_attributes->p_Insert15(t_id,(new c_XMLAttribute)->m_new(t_id,t_value));
	}else{
		t_attribute->m_value=t_value;
	}
}
c_XMLNode* c_XMLNode::p_AddChild(String t_name,String t_attributes,String t_value){
	if(m_valid==false){
		return 0;
	}
	c_XMLNode* t_child=(new c_XMLNode)->m_new(t_name,true);
	gc_assign(t_child->m_doc,m_doc);
	gc_assign(t_child->m_parent,this);
	t_child->m_value=t_value;
	t_child->m_path=m_path+String(L"/",1)+t_child->m_name;
	gc_assign(t_child->m_pathList,m_doc->m_paths->p_Get3(t_child->m_path));
	if(t_child->m_pathList==0){
		gc_assign(t_child->m_pathList,(new c_List5)->m_new());
		m_doc->m_paths->p_Set17(t_child->m_path,t_child->m_pathList);
	}
	gc_assign(t_child->m_pathListNode,t_child->m_pathList->p_AddLast5(t_child));
	if((t_attributes.Length())!=0){
		c_XMLAttributeQuery* t_query=(new c_XMLAttributeQuery)->m_new(t_attributes);
		if(t_query->p_Length()>0){
			for(int t_index=0;t_index<t_query->p_Length();t_index=t_index+1){
				t_child->p_SetAttribute(t_query->m_items[t_index]->m_id,t_query->m_items[t_index]->m_value);
			}
		}
	}
	if((m_lastChild)!=0){
		gc_assign(m_lastChild->m_nextSibling,t_child);
		gc_assign(t_child->m_previousSibling,m_lastChild);
		gc_assign(m_lastChild,t_child);
	}else{
		gc_assign(m_firstChild,t_child);
		gc_assign(m_lastChild,t_child);
	}
	m_children->p_AddLast5(t_child);
	return t_child;
}
c_List5* c_XMLNode::p_GetChildrenAtPath(String t_path){
	c_List5* t_result=(new c_List5)->m_new();
	if(t_path.Length()==0){
		return t_result;
	}
	c_List5* t_pathList=m_doc->m_paths->p_Get3(this->m_path+String(L"/",1)+t_path);
	if(t_pathList==0 || t_pathList->p_IsEmpty()){
		return t_result;
	}
	c_Enumerator10* t_=t_pathList->p_ObjectEnumerator();
	while(t_->p_HasNext()){
		c_XMLNode* t_node=t_->p_NextObject();
		t_result->p_AddLast5(t_node);
	}
	return t_result;
}
c_XMLAttribute* c_XMLNode::p_GetXMLAttribute(String t_id){
	return m_attributes->p_Get3(t_id.ToLower());
}
c_List5* c_XMLNode::p_GetChildrenAtPath2(String t_path,String t_attributes){
	c_List5* t_result=(new c_List5)->m_new();
	if(t_path.Length()==0){
		return t_result;
	}
	c_XMLAttributeQuery* t_query=(new c_XMLAttributeQuery)->m_new(t_attributes);
	c_List5* t_pathList=m_doc->m_paths->p_Get3(this->m_path+String(L"/",1)+t_path);
	if(t_pathList==0 || t_pathList->p_IsEmpty()){
		return t_result;
	}
	c_Enumerator10* t_=t_pathList->p_ObjectEnumerator();
	while(t_->p_HasNext()){
		c_XMLNode* t_node=t_->p_NextObject();
		if(t_query->p_Test(t_node)){
			t_result->p_AddLast5(t_node);
		}
	}
	return t_result;
}
String c_XMLNode::p_GetAttribute(String t_id){
	t_id=t_id.ToLower();
	c_XMLAttribute* t_attribute=m_attributes->p_Get3(t_id);
	if(t_attribute==0){
		return String();
	}
	return t_attribute->m_value;
}
bool c_XMLNode::p_GetAttribute2(String t_id,bool t_defaultValue){
	t_id=t_id.ToLower();
	c_XMLAttribute* t_attribute=m_attributes->p_Get3(t_id);
	if(t_attribute==0){
		return t_defaultValue;
	}
	return t_attribute->m_value==String(L"true",4) || (t_attribute->m_value).ToInt()==1;
}
int c_XMLNode::p_GetAttribute3(String t_id,int t_defaultValue){
	t_id=t_id.ToLower();
	c_XMLAttribute* t_attribute=m_attributes->p_Get3(t_id);
	if(t_attribute==0){
		return t_defaultValue;
	}
	return (t_attribute->m_value).ToInt();
}
Float c_XMLNode::p_GetAttribute4(String t_id,Float t_defaultValue){
	t_id=t_id.ToLower();
	c_XMLAttribute* t_attribute=m_attributes->p_Get3(t_id);
	if(t_attribute==0){
		return t_defaultValue;
	}
	return (t_attribute->m_value).ToFloat();
}
String c_XMLNode::p_GetAttribute5(String t_id,String t_defaultValue){
	t_id=t_id.ToLower();
	c_XMLAttribute* t_attribute=m_attributes->p_Get3(t_id);
	if(t_attribute==0){
		return t_defaultValue;
	}
	return t_attribute->m_value;
}
void c_XMLNode::mark(){
	Object::mark();
	gc_mark_q(m_doc);
	gc_mark_q(m_pathList);
	gc_mark_q(m_pathListNode);
	gc_mark_q(m_parent);
	gc_mark_q(m_attributes);
	gc_mark_q(m_lastChild);
	gc_mark_q(m_nextSibling);
	gc_mark_q(m_previousSibling);
	gc_mark_q(m_firstChild);
	gc_mark_q(m_children);
}
c_XMLDoc::c_XMLDoc(){
	m_nullNode=0;
	m_version=String();
	m_encoding=String();
	m_paths=(new c_StringMap5)->m_new();
}
c_XMLDoc* c_XMLDoc::m_new(String t_name,String t_version,String t_encoding){
	c_XMLNode::m_new2();
	m_valid=true;
	gc_assign(m_doc,this);
	gc_assign(m_nullNode,(new c_XMLNode)->m_new(String(),false));
	gc_assign(m_nullNode->m_doc,this);
	this->m_name=t_name.ToLower();
	this->m_version=t_version;
	this->m_encoding=t_encoding;
	m_path=t_name;
	gc_assign(m_pathList,(new c_List5)->m_new());
	gc_assign(m_pathListNode,m_pathList->p_AddLast5(this));
	m_paths->p_Insert14(m_path,m_pathList);
	return this;
}
c_XMLDoc* c_XMLDoc::m_new2(){
	c_XMLNode::m_new2();
	return this;
}
void c_XMLDoc::mark(){
	c_XMLNode::mark();
	gc_mark_q(m_nullNode);
	gc_mark_q(m_paths);
}
c_XMLStringBuffer::c_XMLStringBuffer(){
	m_chunk=128;
	m_count=0;
	m_data=Array<int >();
	m_dirty=0;
	m_cache=String();
}
c_XMLStringBuffer* c_XMLStringBuffer::m_new(int t_chunk){
	this->m_chunk=t_chunk;
	return this;
}
int c_XMLStringBuffer::p_Length(){
	return m_count;
}
int c_XMLStringBuffer::p_Last2(int t_defaultValue){
	if(m_count==0){
		return t_defaultValue;
	}
	return m_data[m_count-1];
}
void c_XMLStringBuffer::p_Add10(int t_asc){
	if(m_count==m_data.Length()){
		gc_assign(m_data,m_data.Resize(m_data.Length()+m_chunk));
	}
	m_data[m_count]=t_asc;
	m_count+=1;
	m_dirty=1;
}
void c_XMLStringBuffer::p_Add11(String t_text){
	if(t_text.Length()==0){
		return;
	}
	if(m_count+t_text.Length()>=m_data.Length()){
		gc_assign(m_data,m_data.Resize(int(Float(m_data.Length())+Float(m_chunk)*(Float)ceil(Float(t_text.Length())/Float(m_chunk)))));
	}
	for(int t_textIndex=0;t_textIndex<t_text.Length();t_textIndex=t_textIndex+1){
		m_data[m_count]=(int)t_text[t_textIndex];
		m_count+=1;
	}
	m_dirty=1;
}
void c_XMLStringBuffer::p_Add12(String t_text,int t_offset,int t_suggestedLength){
	int t_realLength=t_text.Length()-t_offset;
	if(t_suggestedLength>0 && t_suggestedLength<t_realLength){
		t_realLength=t_suggestedLength;
	}
	if(t_realLength==0){
		return;
	}
	if(m_count+t_realLength>=m_data.Length()){
		gc_assign(m_data,m_data.Resize(int(Float(m_data.Length())+Float(m_chunk)*(Float)ceil(Float(t_realLength)/Float(m_chunk)))));
	}
	for(int t_textIndex=t_offset;t_textIndex<t_offset+t_realLength;t_textIndex=t_textIndex+1){
		m_data[m_count]=(int)t_text[t_textIndex];
		m_count+=1;
	}
	m_dirty=1;
}
String c_XMLStringBuffer::p_value(){
	if((m_dirty)!=0){
		m_dirty=0;
		if(m_count==0){
			m_cache=String();
		}else{
			m_cache=String::FromChars(m_data.Slice(0,m_count));
		}
	}
	return m_cache;
}
void c_XMLStringBuffer::p_Clear(){
	m_count=0;
	m_cache=String();
	m_dirty=0;
}
bool c_XMLStringBuffer::p_Trim(){
	if(m_count==0){
		return false;
	}
	if(m_count==1 && (m_data[0]==32 || m_data[0]==9) || m_count==2 && (m_data[0]==32 || m_data[0]==9) && (m_data[1]==32 || m_data[1]==9)){
		p_Clear();
		return true;
	}
	int t_startIndex=0;
	for(t_startIndex=0;t_startIndex<m_count;t_startIndex=t_startIndex+1){
		if(m_data[t_startIndex]!=32 && m_data[t_startIndex]!=9){
			break;
		}
	}
	if(t_startIndex==m_count){
		p_Clear();
		return true;
	}
	int t_endIndex=0;
	for(t_endIndex=m_count-1;t_endIndex>=0;t_endIndex=t_endIndex+-1){
		if(m_data[t_endIndex]!=32 && m_data[t_endIndex]!=9){
			break;
		}
	}
	if(t_startIndex==0 && t_endIndex==m_count-1){
		return false;
	}
	m_count=t_endIndex-t_startIndex+1;
	if(t_startIndex>0){
		for(int t_trimIndex=0;t_trimIndex<m_count;t_trimIndex=t_trimIndex+1){
			m_data[t_trimIndex]=m_data[t_trimIndex+t_startIndex];
		}
	}
	return true;
}
void c_XMLStringBuffer::mark(){
	Object::mark();
	gc_mark_q(m_data);
}
c_List5::c_List5(){
	m__head=((new c_HeadNode5)->m_new());
}
c_List5* c_List5::m_new(){
	return this;
}
c_Node14* c_List5::p_AddLast5(c_XMLNode* t_data){
	return (new c_Node14)->m_new(m__head,m__head->m__pred,t_data);
}
c_List5* c_List5::m_new2(Array<c_XMLNode* > t_data){
	Array<c_XMLNode* > t_=t_data;
	int t_2=0;
	while(t_2<t_.Length()){
		c_XMLNode* t_t=t_[t_2];
		t_2=t_2+1;
		p_AddLast5(t_t);
	}
	return this;
}
c_XMLNode* c_List5::p_RemoveLast(){
	c_XMLNode* t_data=m__head->m__pred->m__data;
	m__head->m__pred->p_Remove2();
	return t_data;
}
bool c_List5::p_Equals10(c_XMLNode* t_lhs,c_XMLNode* t_rhs){
	return t_lhs==t_rhs;
}
c_Node14* c_List5::p_FindLast10(c_XMLNode* t_value,c_Node14* t_start){
	while(t_start!=m__head){
		if(p_Equals10(t_value,t_start->m__data)){
			return t_start;
		}
		t_start=t_start->m__pred;
	}
	return 0;
}
c_Node14* c_List5::p_FindLast11(c_XMLNode* t_value){
	return p_FindLast10(t_value,m__head->m__pred);
}
void c_List5::p_RemoveLast5(c_XMLNode* t_value){
	c_Node14* t_node=p_FindLast11(t_value);
	if((t_node)!=0){
		t_node->p_Remove2();
	}
}
bool c_List5::p_IsEmpty(){
	return m__head->m__succ==m__head;
}
c_XMLNode* c_List5::p_Last(){
	return m__head->m__pred->m__data;
}
c_Enumerator10* c_List5::p_ObjectEnumerator(){
	return (new c_Enumerator10)->m_new(this);
}
void c_List5::mark(){
	Object::mark();
	gc_mark_q(m__head);
}
c_Node14::c_Node14(){
	m__succ=0;
	m__pred=0;
	m__data=0;
}
c_Node14* c_Node14::m_new(c_Node14* t_succ,c_Node14* t_pred,c_XMLNode* t_data){
	gc_assign(m__succ,t_succ);
	gc_assign(m__pred,t_pred);
	gc_assign(m__succ->m__pred,this);
	gc_assign(m__pred->m__succ,this);
	gc_assign(m__data,t_data);
	return this;
}
c_Node14* c_Node14::m_new2(){
	return this;
}
int c_Node14::p_Remove2(){
	gc_assign(m__succ->m__pred,m__pred);
	gc_assign(m__pred->m__succ,m__succ);
	return 0;
}
void c_Node14::mark(){
	Object::mark();
	gc_mark_q(m__succ);
	gc_mark_q(m__pred);
	gc_mark_q(m__data);
}
c_HeadNode5::c_HeadNode5(){
}
c_HeadNode5* c_HeadNode5::m_new(){
	c_Node14::m_new2();
	gc_assign(m__succ,(this));
	gc_assign(m__pred,(this));
	return this;
}
void c_HeadNode5::mark(){
	c_Node14::mark();
}
bool bb_xml_XMLHasStringAtOffset(String t_needle,String t_haystack,int t_offset){
	if(t_offset+t_needle.Length()>t_haystack.Length()){
		return false;
	}
	for(int t_index=0;t_index<t_needle.Length();t_index=t_index+1){
		if((int)t_needle[t_index]!=(int)t_haystack[t_offset+t_index]){
			return false;
		}
	}
	return true;
}
c_Map10::c_Map10(){
	m_root=0;
}
c_Map10* c_Map10::m_new(){
	return this;
}
int c_Map10::p_RotateLeft11(c_Node15* t_node){
	c_Node15* t_child=t_node->m_right;
	gc_assign(t_node->m_right,t_child->m_left);
	if((t_child->m_left)!=0){
		gc_assign(t_child->m_left->m_parent,t_node);
	}
	gc_assign(t_child->m_parent,t_node->m_parent);
	if((t_node->m_parent)!=0){
		if(t_node==t_node->m_parent->m_left){
			gc_assign(t_node->m_parent->m_left,t_child);
		}else{
			gc_assign(t_node->m_parent->m_right,t_child);
		}
	}else{
		gc_assign(m_root,t_child);
	}
	gc_assign(t_child->m_left,t_node);
	gc_assign(t_node->m_parent,t_child);
	return 0;
}
int c_Map10::p_RotateRight10(c_Node15* t_node){
	c_Node15* t_child=t_node->m_left;
	gc_assign(t_node->m_left,t_child->m_right);
	if((t_child->m_right)!=0){
		gc_assign(t_child->m_right->m_parent,t_node);
	}
	gc_assign(t_child->m_parent,t_node->m_parent);
	if((t_node->m_parent)!=0){
		if(t_node==t_node->m_parent->m_right){
			gc_assign(t_node->m_parent->m_right,t_child);
		}else{
			gc_assign(t_node->m_parent->m_left,t_child);
		}
	}else{
		gc_assign(m_root,t_child);
	}
	gc_assign(t_child->m_right,t_node);
	gc_assign(t_node->m_parent,t_child);
	return 0;
}
int c_Map10::p_InsertFixup10(c_Node15* t_node){
	while(((t_node->m_parent)!=0) && t_node->m_parent->m_color==-1 && ((t_node->m_parent->m_parent)!=0)){
		if(t_node->m_parent==t_node->m_parent->m_parent->m_left){
			c_Node15* t_uncle=t_node->m_parent->m_parent->m_right;
			if(((t_uncle)!=0) && t_uncle->m_color==-1){
				t_node->m_parent->m_color=1;
				t_uncle->m_color=1;
				t_uncle->m_parent->m_color=-1;
				t_node=t_uncle->m_parent;
			}else{
				if(t_node==t_node->m_parent->m_right){
					t_node=t_node->m_parent;
					p_RotateLeft11(t_node);
				}
				t_node->m_parent->m_color=1;
				t_node->m_parent->m_parent->m_color=-1;
				p_RotateRight10(t_node->m_parent->m_parent);
			}
		}else{
			c_Node15* t_uncle2=t_node->m_parent->m_parent->m_left;
			if(((t_uncle2)!=0) && t_uncle2->m_color==-1){
				t_node->m_parent->m_color=1;
				t_uncle2->m_color=1;
				t_uncle2->m_parent->m_color=-1;
				t_node=t_uncle2->m_parent;
			}else{
				if(t_node==t_node->m_parent->m_left){
					t_node=t_node->m_parent;
					p_RotateRight10(t_node);
				}
				t_node->m_parent->m_color=1;
				t_node->m_parent->m_parent->m_color=-1;
				p_RotateLeft11(t_node->m_parent->m_parent);
			}
		}
	}
	m_root->m_color=1;
	return 0;
}
bool c_Map10::p_Set17(String t_key,c_List5* t_value){
	c_Node15* t_node=m_root;
	c_Node15* t_parent=0;
	int t_cmp=0;
	while((t_node)!=0){
		t_parent=t_node;
		t_cmp=p_Compare6(t_key,t_node->m_key);
		if(t_cmp>0){
			t_node=t_node->m_right;
		}else{
			if(t_cmp<0){
				t_node=t_node->m_left;
			}else{
				gc_assign(t_node->m_value,t_value);
				return false;
			}
		}
	}
	t_node=(new c_Node15)->m_new(t_key,t_value,-1,t_parent);
	if((t_parent)!=0){
		if(t_cmp>0){
			gc_assign(t_parent->m_right,t_node);
		}else{
			gc_assign(t_parent->m_left,t_node);
		}
		p_InsertFixup10(t_node);
	}else{
		gc_assign(m_root,t_node);
	}
	return true;
}
bool c_Map10::p_Insert14(String t_key,c_List5* t_value){
	return p_Set17(t_key,t_value);
}
c_Node15* c_Map10::p_FindNode3(String t_key){
	c_Node15* t_node=m_root;
	while((t_node)!=0){
		int t_cmp=p_Compare6(t_key,t_node->m_key);
		if(t_cmp>0){
			t_node=t_node->m_right;
		}else{
			if(t_cmp<0){
				t_node=t_node->m_left;
			}else{
				return t_node;
			}
		}
	}
	return t_node;
}
c_List5* c_Map10::p_Get3(String t_key){
	c_Node15* t_node=p_FindNode3(t_key);
	if((t_node)!=0){
		return t_node->m_value;
	}
	return 0;
}
void c_Map10::mark(){
	Object::mark();
	gc_mark_q(m_root);
}
c_StringMap5::c_StringMap5(){
}
c_StringMap5* c_StringMap5::m_new(){
	c_Map10::m_new();
	return this;
}
int c_StringMap5::p_Compare6(String t_lhs,String t_rhs){
	return t_lhs.Compare(t_rhs);
}
void c_StringMap5::mark(){
	c_Map10::mark();
}
c_Node15::c_Node15(){
	m_key=String();
	m_right=0;
	m_left=0;
	m_value=0;
	m_color=0;
	m_parent=0;
}
c_Node15* c_Node15::m_new(String t_key,c_List5* t_value,int t_color,c_Node15* t_parent){
	this->m_key=t_key;
	gc_assign(this->m_value,t_value);
	this->m_color=t_color;
	gc_assign(this->m_parent,t_parent);
	return this;
}
c_Node15* c_Node15::m_new2(){
	return this;
}
void c_Node15::mark(){
	Object::mark();
	gc_mark_q(m_right);
	gc_mark_q(m_left);
	gc_mark_q(m_value);
	gc_mark_q(m_parent);
}
c_XMLAttributeQuery::c_XMLAttributeQuery(){
	m_count=0;
	m_items=Array<c_XMLAttributeQueryItem* >();
	m_chunk=32;
}
c_XMLAttributeQuery* c_XMLAttributeQuery::m_new(String t_query){
	int t_queryIndex=0;
	int t_queryAsc=0;
	c_XMLStringBuffer* t_buffer=(new c_XMLStringBuffer)->m_new(256);
	bool t_isEscaped=false;
	bool t_processBuffer=false;
	bool t_processItem=false;
	bool t_hasId=false;
	bool t_hasValue=false;
	bool t_hasEquals=false;
	bool t_hasSepcial=false;
	String t_itemId=String();
	String t_itemValue=String();
	for(t_queryIndex=0;t_queryIndex<t_query.Length();t_queryIndex=t_queryIndex+1){
		t_queryAsc=(int)t_query[t_queryIndex];
		if(t_isEscaped){
			t_isEscaped=false;
			t_buffer->p_Add10(t_queryAsc);
		}else{
			int t_1=t_queryAsc;
			if(t_1==38){
				t_processBuffer=true;
				t_processItem=true;
			}else{
				if(t_1==61){
					t_processBuffer=true;
					t_hasEquals=true;
				}else{
					if(t_1==64){
						if(t_hasId==false){
							if(t_buffer->p_Length()==0){
								t_hasSepcial=true;
							}
						}else{
							t_buffer->p_Add10(t_queryAsc);
						}
					}else{
						if(t_1==92){
							t_isEscaped=true;
						}else{
							if(t_hasId || (t_queryAsc==45 || t_queryAsc==95 || t_queryAsc>=48 && t_queryAsc<=57 || t_queryAsc>=65 && t_queryAsc<=90 || t_queryAsc>=97 && t_queryAsc<=122)){
								t_buffer->p_Add10(t_queryAsc);
							}
						}
					}
				}
			}
		}
		if(t_queryIndex==t_query.Length()-1){
			t_processBuffer=true;
			t_processItem=true;
			if(t_isEscaped && t_hasId){
				t_buffer->p_Add10(92);
			}
			if(t_hasEquals && t_buffer->p_Length()==0){
				t_hasValue=true;
			}
		}
		if(t_processBuffer){
			t_processBuffer=false;
			if(t_hasId==false){
				t_itemId=t_buffer->p_value();
				t_buffer->p_Clear();
				t_hasId=t_itemId.Length()>0;
			}else{
				t_itemValue=t_buffer->p_value();
				t_buffer->p_Clear();
				t_hasValue=true;
			}
		}
		if(t_processItem){
			t_processItem=false;
			if(t_hasId){
				if(m_count==m_items.Length()){
					gc_assign(m_items,m_items.Resize(m_items.Length()+m_chunk));
				}
				gc_assign(m_items[m_count],(new c_XMLAttributeQueryItem)->m_new(t_itemId,t_itemValue,t_hasValue,t_hasSepcial));
				m_count+=1;
				t_itemId=String();
				t_itemValue=String();
				t_hasId=false;
				t_hasValue=false;
				t_hasSepcial=false;
			}
		}
	}
	return this;
}
c_XMLAttributeQuery* c_XMLAttributeQuery::m_new2(){
	return this;
}
int c_XMLAttributeQuery::p_Length(){
	return m_count;
}
bool c_XMLAttributeQuery::p_Test(c_XMLNode* t_node){
	c_XMLAttribute* t_attribute=0;
	for(int t_index=0;t_index<m_count;t_index=t_index+1){
		if(m_items[t_index]->m_special==false){
			t_attribute=t_node->p_GetXMLAttribute(m_items[t_index]->m_id);
			if(t_attribute==0 || m_items[t_index]->m_required && t_attribute->m_value!=m_items[t_index]->m_value){
				return false;
			}
		}else{
			String t_2=m_items[t_index]->m_id;
			if(t_2==String(L"value",5)){
				if(m_items[t_index]->m_required && t_node->m_value!=m_items[t_index]->m_value){
					return false;
				}
			}
		}
	}
	return true;
}
void c_XMLAttributeQuery::mark(){
	Object::mark();
	gc_mark_q(m_items);
}
c_XMLAttributeQueryItem::c_XMLAttributeQueryItem(){
	m_id=String();
	m_value=String();
	m_required=false;
	m_special=false;
}
c_XMLAttributeQueryItem* c_XMLAttributeQueryItem::m_new(String t_id,String t_value,bool t_required,bool t_special){
	this->m_id=t_id;
	this->m_value=t_value;
	this->m_required=t_required;
	this->m_special=t_special;
	return this;
}
c_XMLAttributeQueryItem* c_XMLAttributeQueryItem::m_new2(){
	return this;
}
void c_XMLAttributeQueryItem::mark(){
	Object::mark();
}
c_XMLAttribute::c_XMLAttribute(){
	m_id=String();
	m_value=String();
}
c_XMLAttribute* c_XMLAttribute::m_new(String t_id,String t_value){
	this->m_id=t_id;
	this->m_value=t_value;
	return this;
}
c_XMLAttribute* c_XMLAttribute::m_new2(){
	return this;
}
void c_XMLAttribute::mark(){
	Object::mark();
}
c_Map11::c_Map11(){
	m_root=0;
}
c_Map11* c_Map11::m_new(){
	return this;
}
c_Node16* c_Map11::p_FindNode3(String t_key){
	c_Node16* t_node=m_root;
	while((t_node)!=0){
		int t_cmp=p_Compare6(t_key,t_node->m_key);
		if(t_cmp>0){
			t_node=t_node->m_right;
		}else{
			if(t_cmp<0){
				t_node=t_node->m_left;
			}else{
				return t_node;
			}
		}
	}
	return t_node;
}
c_XMLAttribute* c_Map11::p_Get3(String t_key){
	c_Node16* t_node=p_FindNode3(t_key);
	if((t_node)!=0){
		return t_node->m_value;
	}
	return 0;
}
int c_Map11::p_RotateLeft12(c_Node16* t_node){
	c_Node16* t_child=t_node->m_right;
	gc_assign(t_node->m_right,t_child->m_left);
	if((t_child->m_left)!=0){
		gc_assign(t_child->m_left->m_parent,t_node);
	}
	gc_assign(t_child->m_parent,t_node->m_parent);
	if((t_node->m_parent)!=0){
		if(t_node==t_node->m_parent->m_left){
			gc_assign(t_node->m_parent->m_left,t_child);
		}else{
			gc_assign(t_node->m_parent->m_right,t_child);
		}
	}else{
		gc_assign(m_root,t_child);
	}
	gc_assign(t_child->m_left,t_node);
	gc_assign(t_node->m_parent,t_child);
	return 0;
}
int c_Map11::p_RotateRight11(c_Node16* t_node){
	c_Node16* t_child=t_node->m_left;
	gc_assign(t_node->m_left,t_child->m_right);
	if((t_child->m_right)!=0){
		gc_assign(t_child->m_right->m_parent,t_node);
	}
	gc_assign(t_child->m_parent,t_node->m_parent);
	if((t_node->m_parent)!=0){
		if(t_node==t_node->m_parent->m_right){
			gc_assign(t_node->m_parent->m_right,t_child);
		}else{
			gc_assign(t_node->m_parent->m_left,t_child);
		}
	}else{
		gc_assign(m_root,t_child);
	}
	gc_assign(t_child->m_right,t_node);
	gc_assign(t_node->m_parent,t_child);
	return 0;
}
int c_Map11::p_InsertFixup11(c_Node16* t_node){
	while(((t_node->m_parent)!=0) && t_node->m_parent->m_color==-1 && ((t_node->m_parent->m_parent)!=0)){
		if(t_node->m_parent==t_node->m_parent->m_parent->m_left){
			c_Node16* t_uncle=t_node->m_parent->m_parent->m_right;
			if(((t_uncle)!=0) && t_uncle->m_color==-1){
				t_node->m_parent->m_color=1;
				t_uncle->m_color=1;
				t_uncle->m_parent->m_color=-1;
				t_node=t_uncle->m_parent;
			}else{
				if(t_node==t_node->m_parent->m_right){
					t_node=t_node->m_parent;
					p_RotateLeft12(t_node);
				}
				t_node->m_parent->m_color=1;
				t_node->m_parent->m_parent->m_color=-1;
				p_RotateRight11(t_node->m_parent->m_parent);
			}
		}else{
			c_Node16* t_uncle2=t_node->m_parent->m_parent->m_left;
			if(((t_uncle2)!=0) && t_uncle2->m_color==-1){
				t_node->m_parent->m_color=1;
				t_uncle2->m_color=1;
				t_uncle2->m_parent->m_color=-1;
				t_node=t_uncle2->m_parent;
			}else{
				if(t_node==t_node->m_parent->m_left){
					t_node=t_node->m_parent;
					p_RotateRight11(t_node);
				}
				t_node->m_parent->m_color=1;
				t_node->m_parent->m_parent->m_color=-1;
				p_RotateLeft12(t_node->m_parent->m_parent);
			}
		}
	}
	m_root->m_color=1;
	return 0;
}
bool c_Map11::p_Set18(String t_key,c_XMLAttribute* t_value){
	c_Node16* t_node=m_root;
	c_Node16* t_parent=0;
	int t_cmp=0;
	while((t_node)!=0){
		t_parent=t_node;
		t_cmp=p_Compare6(t_key,t_node->m_key);
		if(t_cmp>0){
			t_node=t_node->m_right;
		}else{
			if(t_cmp<0){
				t_node=t_node->m_left;
			}else{
				gc_assign(t_node->m_value,t_value);
				return false;
			}
		}
	}
	t_node=(new c_Node16)->m_new(t_key,t_value,-1,t_parent);
	if((t_parent)!=0){
		if(t_cmp>0){
			gc_assign(t_parent->m_right,t_node);
		}else{
			gc_assign(t_parent->m_left,t_node);
		}
		p_InsertFixup11(t_node);
	}else{
		gc_assign(m_root,t_node);
	}
	return true;
}
bool c_Map11::p_Insert15(String t_key,c_XMLAttribute* t_value){
	return p_Set18(t_key,t_value);
}
void c_Map11::mark(){
	Object::mark();
	gc_mark_q(m_root);
}
c_StringMap6::c_StringMap6(){
}
c_StringMap6* c_StringMap6::m_new(){
	c_Map11::m_new();
	return this;
}
int c_StringMap6::p_Compare6(String t_lhs,String t_rhs){
	return t_lhs.Compare(t_rhs);
}
void c_StringMap6::mark(){
	c_Map11::mark();
}
c_Node16::c_Node16(){
	m_key=String();
	m_right=0;
	m_left=0;
	m_value=0;
	m_color=0;
	m_parent=0;
}
c_Node16* c_Node16::m_new(String t_key,c_XMLAttribute* t_value,int t_color,c_Node16* t_parent){
	this->m_key=t_key;
	gc_assign(this->m_value,t_value);
	this->m_color=t_color;
	gc_assign(this->m_parent,t_parent);
	return this;
}
c_Node16* c_Node16::m_new2(){
	return this;
}
void c_Node16::mark(){
	Object::mark();
	gc_mark_q(m_right);
	gc_mark_q(m_left);
	gc_mark_q(m_value);
	gc_mark_q(m_parent);
}
c_XMLDoc* bb_xml_ParseXML(String t_raw,c_XMLError* t_error,int t_options){
	int t_rawLine=1;
	int t_rawColumn=1;
	int t_rawIndex=0;
	int t_rawAsc=0;
	int t_rawPos=0;
	int t_rawChunkStart=0;
	int t_rawChunkLength=0;
	int t_rawChunkEnd=0;
	String t_rawChunk=String();
	int t_rawChunkIndex=0;
	int t_rawChunkAsc=0;
	c_XMLDoc* t_doc=0;
	c_XMLNode* t_parent=0;
	c_XMLNode* t_current=0;
	c_XMLStringBuffer* t_whitespaceBuffer=(new c_XMLStringBuffer)->m_new(1024);
	c_XMLStringBuffer* t_attributeBuffer=(new c_XMLStringBuffer)->m_new(1024);
	bool t_processAttributeBuffer=false;
	bool t_processTag=false;
	String t_tagName=String();
	String t_formatVersion=String();
	String t_formatEncoding=String();
	String t_attributeId=String();
	String t_attributeValue=String();
	bool t_inTag=false;
	bool t_inQuote=false;
	bool t_inFormat=false;
	bool t_isCloseSelf=false;
	bool t_isSingleAttribute=false;
	bool t_hasFormat=false;
	bool t_hasTagName=false;
	bool t_hasTagClose=false;
	bool t_hasAttributeId=false;
	bool t_hasAttributeValue=false;
	bool t_hasEquals=false;
	bool t_waitTagClose=false;
	c_List5* t_stack=(new c_List5)->m_new();
	int t_quoteAsc=0;
	if((t_error)!=0){
		t_error->p_Reset();
	}
	if(t_raw.Length()==0){
		if((t_error)!=0){
			t_error->p_Set16(String(L"no xml data",11),-1,-1,-1);
		}
		return 0;
	}
	for(t_rawIndex=0;t_rawIndex<t_raw.Length();t_rawIndex=t_rawIndex+1){
		t_rawAsc=(int)t_raw[t_rawIndex];
		if(t_inTag==false){
			int t_3=t_rawAsc;
			if(t_3==9 || t_3==32){
				if(((t_whitespaceBuffer->p_Length())!=0) || ((t_parent)!=0) && ((t_parent->m_value.Length())!=0)){
					int t_lastAsc=t_whitespaceBuffer->p_Last2(-1);
					if((t_options&1)==0 || ((t_whitespaceBuffer->p_Length())!=0) && t_lastAsc!=9 && t_lastAsc!=32){
						if(t_parent==0){
							if((t_error)!=0){
								t_error->p_Set16(String(L"illegal character",17),t_rawLine,t_rawColumn,t_rawIndex);
							}
							return 0;
						}
						t_whitespaceBuffer->p_Add10(t_rawAsc);
					}
				}
				t_rawColumn+=1;
			}else{
				if(t_3==10){
					t_rawLine+=1;
					t_rawColumn=1;
				}else{
					if(t_3==13){
					}else{
						if(t_3==60){
							if(bb_xml_XMLHasStringAtOffset(String(L"<?xml",5),t_raw,t_rawIndex)){
								if(t_hasFormat){
									if((t_error)!=0){
										t_error->p_Set16(String(L"duplicate xml format",20),t_rawLine,t_rawColumn,t_rawIndex);
									}
									return 0;
								}
								t_inTag=true;
								t_inFormat=true;
								t_rawColumn+=String(L"<?xml",5).Length();
								t_rawIndex=t_rawPos+String(L"<?xml",5).Length()-1;
							}else{
								if(bb_xml_XMLHasStringAtOffset(String(L"<!--",4),t_raw,t_rawIndex)){
									t_rawPos=t_raw.Find(String(L"-->",3),t_rawIndex+String(L"<!--",4).Length());
									if(t_rawPos==-1){
										if((t_error)!=0){
											t_error->p_Set16(String(L"comment not closed",18),t_rawLine,t_rawColumn,t_rawIndex);
										}
										return 0;
									}
									t_rawChunkStart=t_rawIndex+String(L"<!--",4).Length();
									t_rawChunkLength=t_rawPos-(t_rawIndex+String(L"<!--",4).Length());
									t_rawChunkEnd=t_rawChunkStart+t_rawChunkLength;
									for(t_rawChunkIndex=t_rawChunkStart;t_rawChunkIndex<t_rawChunkEnd;t_rawChunkIndex=t_rawChunkIndex+1){
										t_rawChunkAsc=(int)t_raw[t_rawChunkIndex];
										if(t_rawChunkAsc==10){
											t_rawLine+=1;
											t_rawColumn=1;
										}else{
											t_rawColumn+=1;
										}
									}
									t_rawIndex=t_rawPos+String(L"-->",3).Length()-1;
								}else{
									if(bb_xml_XMLHasStringAtOffset(String(L"<![CDATA[",9),t_raw,t_rawIndex)){
										t_rawPos=t_raw.Find(String(L"]]>",3),t_rawIndex+String(L"<![CDATA[",9).Length());
										if(t_rawPos==-1){
											if((t_error)!=0){
												t_error->p_Set16(String(L"cdata not closed",16),t_rawLine,t_rawColumn,t_rawIndex);
											}
											return 0;
										}
										if(t_parent==0){
											if((t_error)!=0){
												t_error->p_Set16(String(L"unexepcted cdata",16),t_rawLine,t_rawColumn,t_rawIndex);
											}
											return 0;
										}
										t_rawChunkStart=t_rawIndex+String(L"<![CDATA[",9).Length();
										t_rawChunkLength=t_rawPos-(t_rawIndex+String(L"<![CDATA[",9).Length());
										t_rawChunkEnd=t_rawChunkStart+t_rawChunkLength;
										for(t_rawChunkIndex=t_rawChunkStart;t_rawChunkIndex<t_rawChunkEnd;t_rawChunkIndex=t_rawChunkIndex+1){
											t_rawChunkAsc=(int)t_raw[t_rawChunkIndex];
											if(t_rawChunkAsc==10){
												t_rawLine+=1;
												t_rawColumn=1;
											}else{
												t_rawColumn+=1;
											}
										}
										t_whitespaceBuffer->p_Add12(t_raw,t_rawChunkStart,t_rawChunkLength);
										t_rawIndex=t_rawPos+String(L"]]>",3).Length()-1;
									}else{
										t_inTag=true;
										if((t_whitespaceBuffer->p_Length())!=0){
											if((t_options&1)==0){
												t_parent->m_value=t_parent->m_value+t_whitespaceBuffer->p_value();
												t_whitespaceBuffer->p_Clear();
											}else{
												t_whitespaceBuffer->p_Trim();
												if((t_whitespaceBuffer->p_Length())!=0){
													t_parent->m_value=t_parent->m_value+t_whitespaceBuffer->p_value();
													t_whitespaceBuffer->p_Clear();
												}
											}
										}
										t_rawColumn+=1;
									}
								}
							}
						}else{
							if(t_3==62){
								if((t_error)!=0){
									t_error->p_Set16(String(L"unexpected close bracket",24),t_rawLine,t_rawColumn,t_rawIndex);
								}
								return 0;
							}else{
								if(t_parent==0){
									if((t_error)!=0){
										t_error->p_Set16(String(L"illegal character",17),t_rawLine,t_rawColumn,t_rawIndex);
									}
									return 0;
								}
								t_whitespaceBuffer->p_Add10(t_rawAsc);
								t_rawColumn+=1;
							}
						}
					}
				}
			}
		}else{
			if(t_waitTagClose){
				int t_4=t_rawAsc;
				if(t_4==9){
					t_rawColumn+=1;
				}else{
					if(t_4==10){
						t_rawLine+=1;
						t_rawColumn=1;
					}else{
						if(t_4==13){
						}else{
							if(t_4==32){
								t_rawColumn+=1;
							}else{
								if(t_4==62){
									t_waitTagClose=false;
									t_processTag=true;
								}else{
									if((t_error)!=0){
										t_error->p_Set16(String(L"unexpected character",20),t_rawLine,t_rawColumn,t_rawIndex);
									}
									return 0;
								}
							}
						}
					}
				}
			}else{
				if(t_inQuote==false){
					int t_5=t_rawAsc;
					if(t_5==9){
						t_rawColumn+=1;
						if((t_attributeBuffer->p_Length())!=0){
							t_processAttributeBuffer=true;
						}
					}else{
						if(t_5==10){
							t_rawLine+=1;
							t_rawColumn=1;
							if((t_attributeBuffer->p_Length())!=0){
								t_processAttributeBuffer=true;
							}
						}else{
							if(t_5==13){
							}else{
								if(t_5==32){
									t_rawColumn+=1;
									if((t_attributeBuffer->p_Length())!=0){
										t_processAttributeBuffer=true;
									}
								}else{
									if(t_5==34 || t_5==39){
										t_quoteAsc=t_rawAsc;
										t_inQuote=true;
										if(t_hasTagClose || t_hasTagName==false && t_inFormat==false || t_hasEquals==false || ((t_attributeBuffer->p_Length())!=0)){
											if((t_error)!=0){
												t_error->p_Set16(String(L"unexpected quote",16),t_rawLine,t_rawColumn,t_rawIndex);
											}
											return 0;
										}
										t_rawColumn+=1;
										if((t_attributeBuffer->p_Length())!=0){
											t_processAttributeBuffer=true;
										}
									}else{
										if(t_5==47){
											if(t_hasTagClose || t_hasEquals){
												if((t_error)!=0){
													t_error->p_Set16(String(L"unexpected slash",16),t_rawLine,t_rawColumn,t_rawIndex);
												}
												return 0;
											}
											if(t_hasTagName){
												t_waitTagClose=true;
												t_isCloseSelf=true;
											}
											if((t_attributeBuffer->p_Length())!=0){
												t_processAttributeBuffer=true;
											}
											t_hasTagClose=true;
											t_rawColumn+=1;
										}else{
											if(t_5==61){
												t_rawColumn+=1;
												if(t_hasTagClose || t_hasTagName==false && t_inFormat==false || t_hasEquals || t_hasAttributeId || t_attributeBuffer->p_Length()==0){
													if((t_error)!=0){
														t_error->p_Set16(String(L"unexpected equals",17),t_rawLine,t_rawColumn,t_rawIndex);
													}
													return 0;
												}
												t_processAttributeBuffer=true;
												t_hasEquals=true;
											}else{
												if(t_5==62){
													if(t_hasEquals || t_hasTagName==false && t_attributeBuffer->p_Length()==0){
														if((t_error)!=0){
															t_error->p_Set16(String(L"unexpected close bracket",24),t_rawLine,t_rawColumn,t_rawIndex);
														}
														return 0;
													}
													if((t_attributeBuffer->p_Length())!=0){
														t_processAttributeBuffer=true;
													}
													t_processTag=true;
													t_rawColumn+=1;
												}else{
													if(t_5==63){
														if(t_inFormat==false || t_rawIndex==t_raw.Length()-1 || (int)t_raw[t_rawIndex+1]!=62){
															if((t_error)!=0){
																t_error->p_Set16(String(L"unexpected questionmark",23),t_rawLine,t_rawColumn,t_rawIndex);
															}
															return 0;
														}
														t_processTag=true;
														t_rawIndex+=1;
														t_rawColumn+=1;
													}else{
														if(t_rawAsc==45 || t_rawAsc==95 || t_rawAsc>=48 && t_rawAsc<=57 || t_rawAsc>=65 && t_rawAsc<=90 || t_rawAsc>=97 && t_rawAsc<=122){
															if(t_hasTagClose==true && t_hasTagName==true){
																if((t_error)!=0){
																	t_error->p_Set16(String(L"unexpected character",20),t_rawLine,t_rawColumn,t_rawIndex);
																}
																return 0;
															}
															if(t_hasAttributeId && t_hasEquals==false){
																t_isSingleAttribute=true;
																t_processAttributeBuffer=true;
															}else{
																t_attributeBuffer->p_Add10(t_rawAsc);
															}
															t_rawColumn+=1;
														}else{
															if((t_error)!=0){
																t_error->p_Set16(String(L"illegal character",17),t_rawLine,t_rawColumn,t_rawIndex);
															}
															return 0;
														}
													}
												}
											}
										}
									}
								}
							}
						}
					}
				}else{
					if(t_rawAsc==t_quoteAsc){
						t_inQuote=false;
						t_processAttributeBuffer=true;
					}else{
						t_attributeBuffer->p_Add10(t_rawAsc);
					}
				}
				if(t_processAttributeBuffer){
					t_processAttributeBuffer=false;
					if(t_hasTagName==false && t_inFormat==false){
						if(t_hasTagClose==false){
							t_tagName=t_attributeBuffer->p_value();
							if(t_parent==0){
								if(t_doc==0){
									t_doc=(new c_XMLDoc)->m_new(t_tagName,t_formatVersion,t_formatEncoding);
									gc_assign(t_doc->m_doc,t_doc);
									t_doc->m_parent=0;
									t_doc->m_line=t_rawLine;
									t_doc->m_column=t_rawColumn;
									t_doc->m_offset=t_rawIndex;
									t_current=(t_doc);
								}else{
									if((t_error)!=0){
										t_error->p_Set16(String(L"duplicate root",14),t_rawLine,t_rawColumn,t_rawIndex);
									}
									return 0;
								}
							}else{
								t_current=t_parent->p_AddChild(t_tagName,String(),String());
								t_current->m_line=t_rawLine;
								t_current->m_column=t_rawColumn;
								t_current->m_offset=t_rawIndex;
							}
							t_hasTagName=true;
						}else{
							t_tagName=t_attributeBuffer->p_value().ToLower();
							if(t_parent==0 || t_tagName!=t_parent->m_name){
								if((t_error)!=0){
									t_error->p_Set16(String(L"mismatched end tag",18),t_rawLine,t_rawColumn,t_rawIndex);
								}
								return 0;
							}
							t_waitTagClose=true;
							t_hasTagName=true;
						}
					}else{
						if(t_hasAttributeId==false){
							t_attributeId=t_attributeBuffer->p_value().ToLower();
							t_hasAttributeId=true;
						}else{
							t_attributeValue=t_attributeBuffer->p_value();
							t_hasAttributeValue=true;
						}
						if(t_processTag && t_hasAttributeId || t_hasAttributeId && t_hasAttributeValue || t_isSingleAttribute || t_hasTagClose){
							if(t_inFormat==false){
								t_current->p_SetAttribute(t_attributeId,t_attributeValue);
							}else{
								String t_6=t_attributeId;
								if(t_6==String(L"version",7)){
									t_formatVersion=t_attributeValue;
								}else{
									if(t_6==String(L"encoding",8)){
										t_formatEncoding=t_attributeValue;
									}
								}
							}
							t_attributeId=String();
							t_attributeValue=String();
							t_hasAttributeId=false;
							t_hasAttributeValue=false;
							t_hasEquals=false;
						}
					}
					t_attributeBuffer->p_Clear();
				}
				if(t_isSingleAttribute){
					t_isSingleAttribute=false;
					t_attributeBuffer->p_Add10(t_rawAsc);
				}
			}
			if(t_processTag){
				t_processTag=false;
				if(t_inFormat==false){
					if(t_hasTagClose==false){
						t_parent=t_current;
						t_current=0;
						t_stack->p_AddLast5(t_parent);
					}else{
						if(t_isCloseSelf==false){
							if((t_whitespaceBuffer->p_Length())!=0){
								t_parent->m_value=t_parent->m_value+t_whitespaceBuffer->p_value();
								t_whitespaceBuffer->p_Clear();
							}
							t_stack->p_RemoveLast();
							if(t_stack->p_IsEmpty()){
								t_parent=0;
							}else{
								t_parent=t_stack->p_Last();
							}
						}else{
							t_isCloseSelf=false;
						}
					}
				}else{
					t_hasFormat=true;
					t_inFormat=false;
				}
				t_inTag=false;
				t_hasTagClose=false;
				t_hasTagName=false;
				t_waitTagClose=false;
				t_tagName=String();
			}
		}
	}
	if(t_inTag || ((t_parent)!=0) || t_doc==0){
		if((t_error)!=0){
			t_error->p_Set16(String(L"unexpected end of xml",21),t_rawLine,t_rawColumn,t_rawIndex);
		}
		return 0;
	}
	return t_doc;
}
c_Enumerator10::c_Enumerator10(){
	m__list=0;
	m__curr=0;
}
c_Enumerator10* c_Enumerator10::m_new(c_List5* t_list){
	gc_assign(m__list,t_list);
	gc_assign(m__curr,t_list->m__head->m__succ);
	return this;
}
c_Enumerator10* c_Enumerator10::m_new2(){
	return this;
}
bool c_Enumerator10::p_HasNext(){
	while(m__curr->m__succ->m__pred!=m__curr){
		gc_assign(m__curr,m__curr->m__succ);
	}
	return m__curr!=m__list->m__head;
}
c_XMLNode* c_Enumerator10::p_NextObject(){
	c_XMLNode* t_data=m__curr->m__data;
	gc_assign(m__curr,m__curr->m__succ);
	return t_data;
}
void c_Enumerator10::mark(){
	Object::mark();
	gc_mark_q(m__list);
	gc_mark_q(m__curr);
}
int bb_app_Millisecs(){
	return bb_app__game->Millisecs();
}
int bb_fps_startTime;
int bb_fps_fpsCount;
int bb_fps_currentRate;
void bb_fps_UpdateFps(){
	if(bb_app_Millisecs()-bb_fps_startTime>=1000){
		bb_fps_currentRate=bb_fps_fpsCount;
		bb_fps_fpsCount=0;
		bb_fps_startTime=bb_app_Millisecs();
	}else{
		bb_fps_fpsCount+=1;
	}
}
void bb_functions_ResetMatrix(){
	bb_graphics_SetMatrix(FLOAT(1.0),FLOAT(0.0),FLOAT(0.0),FLOAT(1.0),FLOAT(0.0),FLOAT(0.0));
}
int bb_fps_GetFps(){
	return bb_fps_currentRate;
}
int bb_graphics_DrawImageRect(c_Image* t_image,Float t_x,Float t_y,int t_srcX,int t_srcY,int t_srcWidth,int t_srcHeight,int t_frame){
	c_Frame* t_f=t_image->m_frames[t_frame];
	bb_graphics_context->p_Validate();
	bb_graphics_renderDevice->DrawSurface2(t_image->m_surface,-t_image->m_tx+t_x,-t_image->m_ty+t_y,t_srcX+t_f->m_x,t_srcY+t_f->m_y,t_srcWidth,t_srcHeight);
	return 0;
}
int bb_graphics_DrawImageRect2(c_Image* t_image,Float t_x,Float t_y,int t_srcX,int t_srcY,int t_srcWidth,int t_srcHeight,Float t_rotation,Float t_scaleX,Float t_scaleY,int t_frame){
	c_Frame* t_f=t_image->m_frames[t_frame];
	bb_graphics_PushMatrix();
	bb_graphics_Translate(t_x,t_y);
	bb_graphics_Rotate(t_rotation);
	bb_graphics_Scale(t_scaleX,t_scaleY);
	bb_graphics_Translate(-t_image->m_tx,-t_image->m_ty);
	bb_graphics_context->p_Validate();
	bb_graphics_renderDevice->DrawSurface2(t_image->m_surface,FLOAT(0.0),FLOAT(0.0),t_srcX+t_f->m_x,t_srcY+t_f->m_y,t_srcWidth,t_srcHeight);
	bb_graphics_PopMatrix();
	return 0;
}
int bb_input_KeyHit(int t_key){
	return bb_input_device->p_KeyHit(t_key);
}
c_VAction::c_VAction(){
	m_duration=FLOAT(.0);
	m_active=false;
	m_listener=0;
	m_link=0;
	m_time=FLOAT(.0);
}
c_VAction* c_VAction::m_new(){
	return this;
}
void c_VAction::p_Start(){
	m_active=true;
	if((m_listener)!=0){
		m_listener->p_OnActionEvent(1,this);
	}
}
void c_VAction::p_AddToList(c_List6* t_collection){
	gc_assign(m_link,t_collection->p_AddLast6(this));
}
void c_VAction::p_SetListener(c_VActionEventHandler* t_listener){
	gc_assign(this->m_listener,t_listener);
}
void c_VAction::p_Update4(Float t_dt){
}
void c_VAction::p_IncrementTime(Float t_by){
	m_time+=t_by;
	if(m_time>=m_duration){
		m_time=m_duration;
	}
}
void c_VAction::p_Stop(){
	m_active=false;
	if((m_listener)!=0){
		m_listener->p_OnActionEvent(2,this);
		if((m_link)!=0){
			m_link->p_Remove2();
		}
	}
}
void c_VAction::mark(){
	Object::mark();
	gc_mark_q(m_listener);
	gc_mark_q(m_link);
}
c_VVec2Action::c_VVec2Action(){
	m_pointer=0;
	m_moveBy=0;
	m_easingType=0;
	m_startPosition=0;
	m_lastPosition=0;
}
c_VVec2Action* c_VVec2Action::m_new(c_Vec2* t_vector,Float t_moveX,Float t_moveY,Float t_duration,int t_easingType,bool t_start){
	c_VAction::m_new();
	gc_assign(m_pointer,t_vector);
	gc_assign(m_moveBy,(new c_Vec2)->m_new(t_moveX,t_moveY));
	this->m_duration=t_duration;
	this->m_easingType=t_easingType;
	if(t_start){
		p_Start();
	}
	return this;
}
c_VVec2Action* c_VVec2Action::m_new2(){
	c_VAction::m_new();
	return this;
}
void c_VVec2Action::p_Update4(Float t_dt){
	if(m_active){
		if(!((m_startPosition)!=0)){
			gc_assign(m_startPosition,(new c_Vec2)->m_new2(m_pointer));
			gc_assign(m_lastPosition,(new c_Vec2)->m_new2(m_pointer));
		}
		p_IncrementTime(t_dt);
		Float t_x=bb_ease_Tweening(m_easingType,m_time,m_startPosition->m_x,m_moveBy->m_x,m_duration);
		Float t_y=bb_ease_Tweening(m_easingType,m_time,m_startPosition->m_y,m_moveBy->m_y,m_duration);
		m_pointer->p_Add5(t_x-m_lastPosition->m_x,t_y-m_lastPosition->m_y);
		m_lastPosition->p_Set10(t_x,t_y);
		if(m_time>=m_duration){
			p_Stop();
		}
	}
}
void c_VVec2Action::mark(){
	c_VAction::mark();
	gc_mark_q(m_pointer);
	gc_mark_q(m_moveBy);
	gc_mark_q(m_startPosition);
	gc_mark_q(m_lastPosition);
}
c_List6::c_List6(){
	m__head=((new c_HeadNode6)->m_new());
}
c_List6* c_List6::m_new(){
	return this;
}
c_Node17* c_List6::p_AddLast6(c_VAction* t_data){
	return (new c_Node17)->m_new(m__head,m__head->m__pred,t_data);
}
c_List6* c_List6::m_new2(Array<c_VAction* > t_data){
	Array<c_VAction* > t_=t_data;
	int t_2=0;
	while(t_2<t_.Length()){
		c_VAction* t_t=t_[t_2];
		t_2=t_2+1;
		p_AddLast6(t_t);
	}
	return this;
}
c_Enumerator11* c_List6::p_ObjectEnumerator(){
	return (new c_Enumerator11)->m_new(this);
}
void c_List6::mark(){
	Object::mark();
	gc_mark_q(m__head);
}
c_Node17::c_Node17(){
	m__succ=0;
	m__pred=0;
	m__data=0;
}
c_Node17* c_Node17::m_new(c_Node17* t_succ,c_Node17* t_pred,c_VAction* t_data){
	gc_assign(m__succ,t_succ);
	gc_assign(m__pred,t_pred);
	gc_assign(m__succ->m__pred,this);
	gc_assign(m__pred->m__succ,this);
	gc_assign(m__data,t_data);
	return this;
}
c_Node17* c_Node17::m_new2(){
	return this;
}
int c_Node17::p_Remove2(){
	gc_assign(m__succ->m__pred,m__pred);
	gc_assign(m__pred->m__succ,m__succ);
	return 0;
}
void c_Node17::mark(){
	Object::mark();
	gc_mark_q(m__succ);
	gc_mark_q(m__pred);
	gc_mark_q(m__data);
}
c_HeadNode6::c_HeadNode6(){
}
c_HeadNode6* c_HeadNode6::m_new(){
	c_Node17::m_new2();
	gc_assign(m__succ,(this));
	gc_assign(m__pred,(this));
	return this;
}
void c_HeadNode6::mark(){
	c_Node17::mark();
}
c_Enumerator11::c_Enumerator11(){
	m__list=0;
	m__curr=0;
}
c_Enumerator11* c_Enumerator11::m_new(c_List6* t_list){
	gc_assign(m__list,t_list);
	gc_assign(m__curr,t_list->m__head->m__succ);
	return this;
}
c_Enumerator11* c_Enumerator11::m_new2(){
	return this;
}
bool c_Enumerator11::p_HasNext(){
	while(m__curr->m__succ->m__pred!=m__curr){
		gc_assign(m__curr,m__curr->m__succ);
	}
	return m__curr!=m__list->m__head;
}
c_VAction* c_Enumerator11::p_NextObject(){
	c_VAction* t_data=m__curr->m__data;
	gc_assign(m__curr,m__curr->m__succ);
	return t_data;
}
void c_Enumerator11::mark(){
	Object::mark();
	gc_mark_q(m__list);
	gc_mark_q(m__curr);
}
int bb_graphics_Cls(Float t_r,Float t_g,Float t_b){
	bb_graphics_renderDevice->Cls(t_r,t_g,t_b);
	return 0;
}
void bb_functions_ClearScreenWithColor(c_Color* t_color){
	bb_graphics_Cls(t_color->p_Red()*FLOAT(255.0),t_color->p_Green()*FLOAT(255.0),t_color->p_Blue()*FLOAT(255.0));
}
c_Enumerator12::c_Enumerator12(){
	m__list=0;
	m__curr=0;
}
c_Enumerator12* c_Enumerator12::m_new(c_List4* t_list){
	gc_assign(m__list,t_list);
	gc_assign(m__curr,t_list->m__head->m__succ);
	return this;
}
c_Enumerator12* c_Enumerator12::m_new2(){
	return this;
}
bool c_Enumerator12::p_HasNext(){
	while(m__curr->m__succ->m__pred!=m__curr){
		gc_assign(m__curr,m__curr->m__succ);
	}
	return m__curr!=m__list->m__head;
}
c_VShape* c_Enumerator12::p_NextObject(){
	c_VShape* t_data=m__curr->m__data;
	gc_assign(m__curr,m__curr->m__succ);
	return t_data;
}
void c_Enumerator12::mark(){
	Object::mark();
	gc_mark_q(m__list);
	gc_mark_q(m__curr);
}
int bb_graphics_GetBlend(){
	return bb_graphics_context->m_blend;
}
void bb_functions_ResetBlend(){
	if(bb_graphics_GetBlend()!=0){
		bb_graphics_SetBlend(0);
	}
}
bool bb_ease_initialized;
c_LinearTween::c_LinearTween(){
}
c_LinearTween* c_LinearTween::m_new(){
	return this;
}
Float c_LinearTween::p_Do(Float t_t,Float t_b,Float t_c,Float t_d){
	return t_c*t_t/t_d+t_b;
}
void c_LinearTween::mark(){
	Object::mark();
}
Array<c_Tweener* > bb_ease_TweenFunc;
c_EaseInQuad::c_EaseInQuad(){
}
c_EaseInQuad* c_EaseInQuad::m_new(){
	return this;
}
Float c_EaseInQuad::p_Do(Float t_t,Float t_b,Float t_c,Float t_d){
	t_t/=t_d;
	return t_c*t_t*t_t+t_b;
}
void c_EaseInQuad::mark(){
	Object::mark();
}
c_EaseOutQuad::c_EaseOutQuad(){
}
c_EaseOutQuad* c_EaseOutQuad::m_new(){
	return this;
}
Float c_EaseOutQuad::p_Do(Float t_t,Float t_b,Float t_c,Float t_d){
	t_t/=t_d;
	return -t_c*t_t*(t_t-FLOAT(2.0))+t_b;
}
void c_EaseOutQuad::mark(){
	Object::mark();
}
c_EaseInOutQuad::c_EaseInOutQuad(){
}
c_EaseInOutQuad* c_EaseInOutQuad::m_new(){
	return this;
}
Float c_EaseInOutQuad::p_Do(Float t_t,Float t_b,Float t_c,Float t_d){
	t_t/=t_d/FLOAT(2.0);
	if(t_t<FLOAT(1.0)){
		return t_c/FLOAT(2.0)*t_t*t_t+t_b;
	}
	t_t=t_t-FLOAT(1.0);
	return -t_c/FLOAT(2.0)*(t_t*(t_t-FLOAT(2.0))-FLOAT(1.0))+t_b;
}
void c_EaseInOutQuad::mark(){
	Object::mark();
}
c_EaseInCubic::c_EaseInCubic(){
}
c_EaseInCubic* c_EaseInCubic::m_new(){
	return this;
}
Float c_EaseInCubic::p_Do(Float t_t,Float t_b,Float t_c,Float t_d){
	t_t/=t_d;
	return t_c*t_t*t_t*t_t+t_b;
}
void c_EaseInCubic::mark(){
	Object::mark();
}
c_EaseOutCubic::c_EaseOutCubic(){
}
c_EaseOutCubic* c_EaseOutCubic::m_new(){
	return this;
}
Float c_EaseOutCubic::p_Do(Float t_t,Float t_b,Float t_c,Float t_d){
	t_t/=t_d;
	t_t=t_t-FLOAT(1.0);
	return t_c*(t_t*t_t*t_t+FLOAT(1.0))+t_b;
}
void c_EaseOutCubic::mark(){
	Object::mark();
}
c_EaseInOutCubic::c_EaseInOutCubic(){
}
c_EaseInOutCubic* c_EaseInOutCubic::m_new(){
	return this;
}
Float c_EaseInOutCubic::p_Do(Float t_t,Float t_b,Float t_c,Float t_d){
	t_t/=t_d/FLOAT(2.0);
	if(t_t<FLOAT(1.0)){
		return t_c/FLOAT(2.0)*t_t*t_t*t_t+t_b;
	}
	t_t=t_t-FLOAT(2.0);
	return t_c/FLOAT(2.0)*(t_t*t_t*t_t+FLOAT(2.0))+t_b;
}
void c_EaseInOutCubic::mark(){
	Object::mark();
}
c_EaseInQuart::c_EaseInQuart(){
}
c_EaseInQuart* c_EaseInQuart::m_new(){
	return this;
}
Float c_EaseInQuart::p_Do(Float t_t,Float t_b,Float t_c,Float t_d){
	t_t/=t_d;
	return t_c*t_t*t_t*t_t*t_t+t_b;
}
void c_EaseInQuart::mark(){
	Object::mark();
}
c_EaseOutQuart::c_EaseOutQuart(){
}
c_EaseOutQuart* c_EaseOutQuart::m_new(){
	return this;
}
Float c_EaseOutQuart::p_Do(Float t_t,Float t_b,Float t_c,Float t_d){
	t_t/=t_d;
	t_t=t_t-FLOAT(1.0);
	return -t_c*(t_t*t_t*t_t*t_t-FLOAT(1.0))+t_b;
}
void c_EaseOutQuart::mark(){
	Object::mark();
}
c_EaseInOutQuart::c_EaseInOutQuart(){
}
c_EaseInOutQuart* c_EaseInOutQuart::m_new(){
	return this;
}
Float c_EaseInOutQuart::p_Do(Float t_t,Float t_b,Float t_c,Float t_d){
	t_t/=t_d/FLOAT(2.0);
	if(t_t<FLOAT(1.0)){
		return t_c/FLOAT(2.0)*t_t*t_t*t_t*t_t+t_b;
	}
	t_t=t_t-FLOAT(2.0);
	return -t_c/FLOAT(2.0)*(t_t*t_t*t_t*t_t-FLOAT(2.0))+t_b;
}
void c_EaseInOutQuart::mark(){
	Object::mark();
}
c_EaseInQuint::c_EaseInQuint(){
}
c_EaseInQuint* c_EaseInQuint::m_new(){
	return this;
}
Float c_EaseInQuint::p_Do(Float t_t,Float t_b,Float t_c,Float t_d){
	t_t/=t_d;
	return t_c*t_t*t_t*t_t*t_t*t_t+t_b;
}
void c_EaseInQuint::mark(){
	Object::mark();
}
c_EaseOutQuint::c_EaseOutQuint(){
}
c_EaseOutQuint* c_EaseOutQuint::m_new(){
	return this;
}
Float c_EaseOutQuint::p_Do(Float t_t,Float t_b,Float t_c,Float t_d){
	t_t/=t_d;
	t_t=t_t-FLOAT(1.0);
	return t_c*(t_t*t_t*t_t*t_t*t_t+FLOAT(1.0))+t_b;
}
void c_EaseOutQuint::mark(){
	Object::mark();
}
c_EaseInOutQuint::c_EaseInOutQuint(){
}
c_EaseInOutQuint* c_EaseInOutQuint::m_new(){
	return this;
}
Float c_EaseInOutQuint::p_Do(Float t_t,Float t_b,Float t_c,Float t_d){
	t_t/=t_d/FLOAT(2.0);
	if(t_t<FLOAT(1.0)){
		return t_c/FLOAT(2.0)*t_t*t_t*t_t*t_t*t_t+t_b;
	}
	t_t=t_t-FLOAT(2.0);
	return t_c/FLOAT(2.0)*(t_t*t_t*t_t*t_t*t_t+FLOAT(2.0))+t_b;
}
void c_EaseInOutQuint::mark(){
	Object::mark();
}
c_EaseInSine::c_EaseInSine(){
}
c_EaseInSine* c_EaseInSine::m_new(){
	return this;
}
Float c_EaseInSine::p_Do(Float t_t,Float t_b,Float t_c,Float t_d){
	return -t_c*(Float)cos((t_t/t_d*FLOAT(1.5707963250000001)*FLOAT(57.2957795))*D2R)+t_c+t_b;
}
void c_EaseInSine::mark(){
	Object::mark();
}
c_EaseOutSine::c_EaseOutSine(){
}
c_EaseOutSine* c_EaseOutSine::m_new(){
	return this;
}
Float c_EaseOutSine::p_Do(Float t_t,Float t_b,Float t_c,Float t_d){
	return t_c*(Float)sin((t_t/t_d*FLOAT(1.5707963250000001)*FLOAT(57.2957795))*D2R)+t_b;
}
void c_EaseOutSine::mark(){
	Object::mark();
}
c_EaseInOutSine::c_EaseInOutSine(){
}
c_EaseInOutSine* c_EaseInOutSine::m_new(){
	return this;
}
Float c_EaseInOutSine::p_Do(Float t_t,Float t_b,Float t_c,Float t_d){
	return -t_c/FLOAT(2.0)*((Float)cos((FLOAT(3.14159265)*t_t/t_d*FLOAT(57.2957795))*D2R)-FLOAT(1.0))+t_b;
}
void c_EaseInOutSine::mark(){
	Object::mark();
}
c_EaseInExpo::c_EaseInExpo(){
}
c_EaseInExpo* c_EaseInExpo::m_new(){
	return this;
}
Float c_EaseInExpo::p_Do(Float t_t,Float t_b,Float t_c,Float t_d){
	return t_c*(Float)pow(FLOAT(2.0),FLOAT(10.0)*(t_t/t_d-FLOAT(1.0)))+t_b;
}
void c_EaseInExpo::mark(){
	Object::mark();
}
c_EaseOutExpo::c_EaseOutExpo(){
}
c_EaseOutExpo* c_EaseOutExpo::m_new(){
	return this;
}
Float c_EaseOutExpo::p_Do(Float t_t,Float t_b,Float t_c,Float t_d){
	return t_c*(-(Float)pow(FLOAT(2.0),FLOAT(-10.0)*t_t/t_d)+FLOAT(1.0))+t_b;
}
void c_EaseOutExpo::mark(){
	Object::mark();
}
c_EaseInOutExpo::c_EaseInOutExpo(){
}
c_EaseInOutExpo* c_EaseInOutExpo::m_new(){
	return this;
}
Float c_EaseInOutExpo::p_Do(Float t_t,Float t_b,Float t_c,Float t_d){
	t_t/=t_d/FLOAT(2.0);
	if(t_t<FLOAT(1.0)){
		return t_c/FLOAT(2.0)*(Float)pow(FLOAT(2.0),FLOAT(10.0)*(t_t-FLOAT(1.0)))+t_b;
	}
	t_t=t_t-FLOAT(1.0);
	return t_c/FLOAT(2.0)*(-(Float)pow(FLOAT(2.0),FLOAT(-10.0)*t_t)+FLOAT(2.0))+t_b;
}
void c_EaseInOutExpo::mark(){
	Object::mark();
}
c_EaseInCirc::c_EaseInCirc(){
}
c_EaseInCirc* c_EaseInCirc::m_new(){
	return this;
}
Float c_EaseInCirc::p_Do(Float t_t,Float t_b,Float t_c,Float t_d){
	t_t/=t_d;
	return -t_c*((Float)sqrt(FLOAT(1.0)-t_t*t_t)-FLOAT(1.0))+t_b;
}
void c_EaseInCirc::mark(){
	Object::mark();
}
c_EaseOutCirc::c_EaseOutCirc(){
}
c_EaseOutCirc* c_EaseOutCirc::m_new(){
	return this;
}
Float c_EaseOutCirc::p_Do(Float t_t,Float t_b,Float t_c,Float t_d){
	t_t/=t_d;
	t_t=t_t-FLOAT(1.0);
	return t_c*(Float)sqrt(FLOAT(1.0)-t_t*t_t)+t_b;
}
void c_EaseOutCirc::mark(){
	Object::mark();
}
c_EaseInOutCirc::c_EaseInOutCirc(){
}
c_EaseInOutCirc* c_EaseInOutCirc::m_new(){
	return this;
}
Float c_EaseInOutCirc::p_Do(Float t_t,Float t_b,Float t_c,Float t_d){
	t_t/=t_d/FLOAT(2.0);
	if(t_t<FLOAT(1.0)){
		return -t_c/FLOAT(2.0)*((Float)sqrt(FLOAT(1.0)-t_t*t_t)-FLOAT(1.0))+t_b;
	}
	t_t=t_t-FLOAT(2.0);
	return t_c/FLOAT(2.0)*((Float)sqrt(FLOAT(1.0)-t_t*t_t)+FLOAT(1.0))+t_b;
}
void c_EaseInOutCirc::mark(){
	Object::mark();
}
c_EaseInBack::c_EaseInBack(){
}
c_EaseInBack* c_EaseInBack::m_new(){
	return this;
}
Float c_EaseInBack::p_Do(Float t_t,Float t_b,Float t_c,Float t_d){
	t_t/=t_d;
	return t_c*t_t*t_t*((bb_ease_Bounce+FLOAT(1.0))*t_t-bb_ease_Bounce)+t_b;
}
void c_EaseInBack::mark(){
	Object::mark();
}
c_EaseOutBack::c_EaseOutBack(){
}
c_EaseOutBack* c_EaseOutBack::m_new(){
	return this;
}
Float c_EaseOutBack::p_Do(Float t_t,Float t_b,Float t_c,Float t_d){
	t_t=t_t/t_d-FLOAT(1.0);
	return t_c*(t_t*t_t*((bb_ease_Bounce+FLOAT(1.0))*t_t+bb_ease_Bounce)+FLOAT(1.0))+t_b;
}
void c_EaseOutBack::mark(){
	Object::mark();
}
c_EaseInOutBack::c_EaseInOutBack(){
}
c_EaseInOutBack* c_EaseInOutBack::m_new(){
	return this;
}
Float c_EaseInOutBack::p_Do(Float t_t,Float t_b,Float t_c,Float t_d){
	t_t/=t_d/FLOAT(2.0);
	if(t_t<FLOAT(1.0)){
		bb_ease_Bounce*=FLOAT(2.5249999999999999);
		return t_c/FLOAT(2.0)*(t_t*t_t*(bb_ease_Bounce*t_t-bb_ease_Bounce))+t_b;
	}
	t_t=t_t-FLOAT(2.0);
	bb_ease_Bounce*=FLOAT(2.5249999999999999);
	return t_c/FLOAT(2.0)*(t_t*t_t*(bb_ease_Bounce*t_t+bb_ease_Bounce)+FLOAT(2.0))+t_b;
}
void c_EaseInOutBack::mark(){
	Object::mark();
}
c_EaseInBounce::c_EaseInBounce(){
}
c_EaseInBounce* c_EaseInBounce::m_new(){
	return this;
}
Float c_EaseInBounce::p_Do(Float t_t,Float t_b,Float t_c,Float t_d){
	return t_c-bb_ease_Tweening(26,t_d-t_t,FLOAT(0.0),t_c,t_d)+t_b;
}
void c_EaseInBounce::mark(){
	Object::mark();
}
c_EaseOutBounce::c_EaseOutBounce(){
}
c_EaseOutBounce* c_EaseOutBounce::m_new(){
	return this;
}
Float c_EaseOutBounce::p_Do(Float t_t,Float t_b,Float t_c,Float t_d){
	t_t/=t_d;
	if(t_t<FLOAT(0.36363636363636365)){
		return t_c*(FLOAT(7.5625)*t_t*t_t)+t_b;
	}else{
		if(t_t<FLOAT(0.72727272727272729)){
			t_t-=FLOAT(0.54545454545454541);
			return t_c*(FLOAT(7.5625)*t_t*t_t+FLOAT(.75))+t_b;
		}else{
			if(t_t<FLOAT(0.90909090909090906)){
				t_t-=FLOAT(0.81818181818181823);
				return t_c*(FLOAT(7.5625)*t_t*t_t+FLOAT(.9375))+t_b;
			}else{
				t_t-=FLOAT(0.95454545454545459);
				return t_c*(FLOAT(7.5625)*t_t*t_t+FLOAT(.984375))+t_b;
			}
		}
	}
}
void c_EaseOutBounce::mark(){
	Object::mark();
}
c_EaseInOutBounce::c_EaseInOutBounce(){
}
c_EaseInOutBounce* c_EaseInOutBounce::m_new(){
	return this;
}
Float c_EaseInOutBounce::p_Do(Float t_t,Float t_b,Float t_c,Float t_d){
	if(t_t<t_d/FLOAT(2.0)){
		return bb_ease_Tweening(25,t_t*FLOAT(2.0),FLOAT(0.0),t_c,t_d)*FLOAT(.5)+t_b;
	}
	return bb_ease_Tweening(26,t_t*FLOAT(2.0)-t_d,FLOAT(0.0),t_c,t_d)*FLOAT(.5)+t_c*FLOAT(.5)+t_b;
}
void c_EaseInOutBounce::mark(){
	Object::mark();
}
c_EaseInElastic::c_EaseInElastic(){
}
c_EaseInElastic* c_EaseInElastic::m_new(){
	return this;
}
Float c_EaseInElastic::p_Do(Float t_t,Float t_b,Float t_c,Float t_d){
	Float t_s=FLOAT(.0);
	if(t_t==FLOAT(0.0)){
		return t_b;
	}
	t_t/=t_d;
	if(t_t==FLOAT(1.0)){
		return t_b+t_c;
	}
	if(!((bb_ease_Power)!=0)){
		bb_ease_Power=t_d*FLOAT(.3);
	}
	if(!((bb_ease_Amplitude)!=0) || bb_ease_Amplitude<bb_math_Abs2(t_c)){
		bb_ease_Amplitude=t_c;
		t_s=bb_ease_Power/FLOAT(4.0);
	}else{
		t_s=bb_ease_Power/FLOAT(6.2831853000000004)*(Float)(asin(t_c/bb_ease_Amplitude)*R2D);
	}
	t_t=t_t-FLOAT(1.0);
	return -(bb_ease_Amplitude*(Float)pow(FLOAT(2.0),FLOAT(10.0)*t_t)*(Float)sin(((t_t*t_d-t_s)*FLOAT(6.2831853000000004)/bb_ease_Power)*D2R))+t_b;
}
void c_EaseInElastic::mark(){
	Object::mark();
}
c_EaseOutElastic::c_EaseOutElastic(){
}
c_EaseOutElastic* c_EaseOutElastic::m_new(){
	return this;
}
Float c_EaseOutElastic::p_Do(Float t_t,Float t_b,Float t_c,Float t_d){
	Float t_s=FLOAT(.0);
	if(t_t==FLOAT(0.0)){
		return t_b;
	}
	t_t/=t_d;
	if(t_t==FLOAT(1.0)){
		return t_b+t_c;
	}
	if(!((bb_ease_Power)!=0)){
		bb_ease_Power=t_d*FLOAT(.3);
	}
	if(!((bb_ease_Amplitude)!=0) || bb_ease_Amplitude<bb_math_Abs2(t_c)){
		bb_ease_Amplitude=t_c;
		t_s=bb_ease_Power/FLOAT(4.0);
	}else{
		t_s=bb_ease_Power/FLOAT(6.2831853000000004)*(Float)(asin(t_c/bb_ease_Amplitude)*R2D);
	}
	return bb_ease_Amplitude*(Float)pow(FLOAT(2.0),FLOAT(-10.0)*t_t)*(Float)sin(((t_t*t_d-t_s)*FLOAT(6.2831853000000004)/bb_ease_Power)*D2R)+t_c+t_b;
}
void c_EaseOutElastic::mark(){
	Object::mark();
}
c_EaseInOutElastic::c_EaseInOutElastic(){
}
c_EaseInOutElastic* c_EaseInOutElastic::m_new(){
	return this;
}
Float c_EaseInOutElastic::p_Do(Float t_t,Float t_b,Float t_c,Float t_d){
	Float t_s=FLOAT(.0);
	if(t_t==FLOAT(0.0)){
		return t_b;
	}
	t_t/=t_d/FLOAT(2.0);
	if(t_t==FLOAT(2.0)){
		return t_b+t_c;
	}
	if(!((bb_ease_Power)!=0)){
		bb_ease_Power=t_d*FLOAT(.3)*FLOAT(1.5);
	}
	if(!((bb_ease_Amplitude)!=0) || bb_ease_Amplitude<bb_math_Abs2(t_c)){
		bb_ease_Amplitude=t_c;
		t_s=bb_ease_Power/FLOAT(4.0);
	}else{
		t_s=bb_ease_Power/FLOAT(6.2831853000000004)*(Float)(asin(t_c/bb_ease_Amplitude)*R2D);
	}
	if(t_t<FLOAT(1.0)){
		t_t=t_t-FLOAT(1.0);
		return FLOAT(-0.5)*(bb_ease_Amplitude*(Float)pow(FLOAT(2.0),FLOAT(10.0)*t_t)*(Float)sin(((t_t*t_d-t_s)*FLOAT(6.2831853000000004)/bb_ease_Power)*D2R))+t_b;
	}
	t_t=t_t-FLOAT(1.0);
	return bb_ease_Amplitude*(Float)pow(FLOAT(2.0),FLOAT(-10.0)*t_t)*(Float)sin(((t_t*t_d-t_s)*FLOAT(6.2831853000000004)/bb_ease_Power)*D2R)*FLOAT(0.5)+t_c+t_b;
}
void c_EaseInOutElastic::mark(){
	Object::mark();
}
void bb_ease_InitTweenSystem(){
	gc_assign(bb_ease_TweenFunc[0],((new c_LinearTween)->m_new()));
	gc_assign(bb_ease_TweenFunc[1],((new c_EaseInQuad)->m_new()));
	gc_assign(bb_ease_TweenFunc[2],((new c_EaseOutQuad)->m_new()));
	gc_assign(bb_ease_TweenFunc[3],((new c_EaseInOutQuad)->m_new()));
	gc_assign(bb_ease_TweenFunc[4],((new c_EaseInCubic)->m_new()));
	gc_assign(bb_ease_TweenFunc[5],((new c_EaseOutCubic)->m_new()));
	gc_assign(bb_ease_TweenFunc[6],((new c_EaseInOutCubic)->m_new()));
	gc_assign(bb_ease_TweenFunc[7],((new c_EaseInQuart)->m_new()));
	gc_assign(bb_ease_TweenFunc[8],((new c_EaseOutQuart)->m_new()));
	gc_assign(bb_ease_TweenFunc[9],((new c_EaseInOutQuart)->m_new()));
	gc_assign(bb_ease_TweenFunc[10],((new c_EaseInQuint)->m_new()));
	gc_assign(bb_ease_TweenFunc[11],((new c_EaseOutQuint)->m_new()));
	gc_assign(bb_ease_TweenFunc[12],((new c_EaseInOutQuint)->m_new()));
	gc_assign(bb_ease_TweenFunc[13],((new c_EaseInSine)->m_new()));
	gc_assign(bb_ease_TweenFunc[14],((new c_EaseOutSine)->m_new()));
	gc_assign(bb_ease_TweenFunc[15],((new c_EaseInOutSine)->m_new()));
	gc_assign(bb_ease_TweenFunc[16],((new c_EaseInExpo)->m_new()));
	gc_assign(bb_ease_TweenFunc[17],((new c_EaseOutExpo)->m_new()));
	gc_assign(bb_ease_TweenFunc[18],((new c_EaseInOutExpo)->m_new()));
	gc_assign(bb_ease_TweenFunc[16],((new c_EaseInExpo)->m_new()));
	gc_assign(bb_ease_TweenFunc[17],((new c_EaseOutExpo)->m_new()));
	gc_assign(bb_ease_TweenFunc[18],((new c_EaseInOutExpo)->m_new()));
	gc_assign(bb_ease_TweenFunc[19],((new c_EaseInCirc)->m_new()));
	gc_assign(bb_ease_TweenFunc[20],((new c_EaseOutCirc)->m_new()));
	gc_assign(bb_ease_TweenFunc[21],((new c_EaseInOutCirc)->m_new()));
	gc_assign(bb_ease_TweenFunc[22],((new c_EaseInBack)->m_new()));
	gc_assign(bb_ease_TweenFunc[23],((new c_EaseOutBack)->m_new()));
	gc_assign(bb_ease_TweenFunc[24],((new c_EaseInOutBack)->m_new()));
	gc_assign(bb_ease_TweenFunc[25],((new c_EaseInBounce)->m_new()));
	gc_assign(bb_ease_TweenFunc[26],((new c_EaseOutBounce)->m_new()));
	gc_assign(bb_ease_TweenFunc[27],((new c_EaseInOutBounce)->m_new()));
	gc_assign(bb_ease_TweenFunc[28],((new c_EaseInElastic)->m_new()));
	gc_assign(bb_ease_TweenFunc[29],((new c_EaseOutElastic)->m_new()));
	gc_assign(bb_ease_TweenFunc[30],((new c_EaseInOutElastic)->m_new()));
}
Float bb_ease_Tweening(int t_type,Float t_t,Float t_b,Float t_c,Float t_d){
	if(!bb_ease_initialized){
		bb_ease_InitTweenSystem();
	}
	return bb_ease_TweenFunc[t_type]->p_Do(t_t,t_b,t_c,t_d);
}
Float bb_ease_Bounce;
Float bb_ease_Power;
Float bb_ease_Amplitude;
int bbInit(){
	GC_CTOR
	bb_random_Seed=1234;
	c_Color::m_Black=(new c_ImmutableColor)->m_new3(FLOAT(0.0),FLOAT(0.0),FLOAT(0.0),FLOAT(1.0));
	c_Color::m_White=(new c_ImmutableColor)->m_new3(FLOAT(1.0),FLOAT(1.0),FLOAT(1.0),FLOAT(1.0));
	c_Color::m_PureRed=(new c_ImmutableColor)->m_new3(FLOAT(1.0),FLOAT(0.0),FLOAT(0.0),FLOAT(1.0));
	c_Color::m_PureGreen=(new c_ImmutableColor)->m_new3(FLOAT(0.0),FLOAT(1.0),FLOAT(0.0),FLOAT(1.0));
	c_Color::m_PureBlue=(new c_ImmutableColor)->m_new3(FLOAT(0.0),FLOAT(0.0),FLOAT(1.0),FLOAT(1.0));
	c_Color::m_Navy=(new c_ImmutableColor)->m_new2(7999);
	c_Color::m_NewBlue=(new c_ImmutableColor)->m_new2(29913);
	c_Color::m_Aqua=(new c_ImmutableColor)->m_new2(8379391);
	c_Color::m_Teal=(new c_ImmutableColor)->m_new2(3787980);
	c_Color::m_Olive=(new c_ImmutableColor)->m_new2(4036976);
	c_Color::m_NewGreen=(new c_ImmutableColor)->m_new2(3066944);
	c_Color::m_Lime=(new c_ImmutableColor)->m_new2(130928);
	c_Color::m_Yellow=(new c_ImmutableColor)->m_new2(16768000);
	c_Color::m_Orange=(new c_ImmutableColor)->m_new2(16745755);
	c_Color::m_NewRed=(new c_ImmutableColor)->m_new2(16728374);
	c_Color::m_Maroon=(new c_ImmutableColor)->m_new2(8721483);
	c_Color::m_Fuchsia=(new c_ImmutableColor)->m_new2(15733438);
	c_Color::m_Purple=(new c_ImmutableColor)->m_new2(11603401);
	c_Color::m_Silver=(new c_ImmutableColor)->m_new2(14540253);
	c_Color::m_Gray=(new c_ImmutableColor)->m_new2(11184810);
	c_Color::m_NewBlack=(new c_ImmutableColor)->m_new2(1118481);
	bb_graphics_context=(new c_GraphicsContext)->m_new();
	bb_graphics_renderDevice=0;
	c_Image::m_DefaultFlags=0;
	c_ImageCache::m_ImageCache=(new c_StringMap3)->m_new();
	bb_graphics_device=0;
	c_Deque::m_NIL=0;
	c_Deque2::m_NIL=0;
	c_Deque3::m_NIL=String();
	c_Stack::m_NIL=0;
	c_Stack2::m_NIL=0;
	c_Stack3::m_NIL=String();
	bb_reflection__classes=Array<c_ClassInfo* >();
	bb_reflection__boolClass=0;
	bb_reflection__intClass=0;
	bb_reflection__floatClass=0;
	bb_reflection__stringClass=0;
	bb_reflection__consts=Array<c_ConstInfo* >();
	bb_reflection__globals=Array<c_GlobalInfo* >();
	bb_reflection__functions=Array<c_FunctionInfo* >();
	bb_reflection__getClass=0;
	bb_reflection__init=bb_reflection___init();
	bb_app__app=0;
	bb_app__delegate=0;
	bb_app__game=BBGame::Game();
	bb_app2_Vsat=0;
	bb_audio_device=0;
	bb_input_device=0;
	bb_app__devWidth=0;
	bb_app__devHeight=0;
	bb_app__displayModes=Array<c_DisplayMode* >();
	bb_app__desktopMode=0;
	bb_app__updateRate=0;
	c_FontCache::m_Cache=(new c_StringMap4)->m_new();
	c_AngelFont::m_firstKp=0;
	c_AngelFont::m__list=(new c_StringMap4)->m_new();
	bb_fps_startTime=0;
	bb_fps_fpsCount=0;
	bb_fps_currentRate=0;
	c_AngelFont::m_secondKp=0;
	bb_ease_initialized=false;
	bb_ease_TweenFunc=Array<c_Tweener* >(31);
	bb_ease_Bounce=FLOAT(1.70158);
	bb_ease_Power=FLOAT(1.0);
	bb_ease_Amplitude=FLOAT(1.0);
	return 0;
}
void gc_mark(){
	gc_mark_q(c_Color::m_Black);
	gc_mark_q(c_Color::m_White);
	gc_mark_q(c_Color::m_PureRed);
	gc_mark_q(c_Color::m_PureGreen);
	gc_mark_q(c_Color::m_PureBlue);
	gc_mark_q(c_Color::m_Navy);
	gc_mark_q(c_Color::m_NewBlue);
	gc_mark_q(c_Color::m_Aqua);
	gc_mark_q(c_Color::m_Teal);
	gc_mark_q(c_Color::m_Olive);
	gc_mark_q(c_Color::m_NewGreen);
	gc_mark_q(c_Color::m_Lime);
	gc_mark_q(c_Color::m_Yellow);
	gc_mark_q(c_Color::m_Orange);
	gc_mark_q(c_Color::m_NewRed);
	gc_mark_q(c_Color::m_Maroon);
	gc_mark_q(c_Color::m_Fuchsia);
	gc_mark_q(c_Color::m_Purple);
	gc_mark_q(c_Color::m_Silver);
	gc_mark_q(c_Color::m_Gray);
	gc_mark_q(c_Color::m_NewBlack);
	gc_mark_q(bb_graphics_context);
	gc_mark_q(bb_graphics_renderDevice);
	gc_mark_q(c_ImageCache::m_ImageCache);
	gc_mark_q(bb_graphics_device);
	gc_mark_q(bb_reflection__classes);
	gc_mark_q(bb_reflection__boolClass);
	gc_mark_q(bb_reflection__intClass);
	gc_mark_q(bb_reflection__floatClass);
	gc_mark_q(bb_reflection__stringClass);
	gc_mark_q(bb_reflection__consts);
	gc_mark_q(bb_reflection__globals);
	gc_mark_q(bb_reflection__functions);
	gc_mark_q(bb_reflection__getClass);
	gc_mark_q(bb_app__app);
	gc_mark_q(bb_app__delegate);
	gc_mark_q(bb_app2_Vsat);
	gc_mark_q(bb_audio_device);
	gc_mark_q(bb_input_device);
	gc_mark_q(bb_app__displayModes);
	gc_mark_q(bb_app__desktopMode);
	gc_mark_q(c_FontCache::m_Cache);
	gc_mark_q(c_AngelFont::m_firstKp);
	gc_mark_q(c_AngelFont::m__list);
	gc_mark_q(c_AngelFont::m_secondKp);
	gc_mark_q(bb_ease_TweenFunc);
}
//${TRANSCODE_END}

int main( int argc,const char *argv[] ){

	BBMonkeyGame::Main( argc,argv );
}
