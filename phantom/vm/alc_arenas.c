/**
 *
 * Phantom OS
 *
 * Copyright (C) 2005-2019 Dmitry Zavalishin, dz@dz.ru
 *
 * VM memory allocator: Arenas machinery
 * 
 * Each allocation is done out of one of the arenas.
 * All the persistent memory is a linked list of arenas.
 * Each arena is prefixed with quite usual Phantom object 
 * (though with no class and with a special flag )
 * which contains arena header.
 *
 * We keep set of arenas which are used for current allocations.
 * 
 * TODO on arena load - init mutex, on unload - deinit
 * 
**/


#define DEBUG_MSG_PREFIX "vm.alloc"
#include <debug_ext.h>
#define debug_level_flow 10
#define debug_level_error 10
#define debug_level_info 10

#include <vm/alloc.h>
#include <vm/object_flags.h>

#include <vm/internal_da.h>

#include <kernel/stats.h>
#include <kernel/page.h>
#include <kernel/vm.h>
#include <kernel/debug_graphical.h>


// ------------------------------------------------------------
// Public data
// ------------------------------------------------------------


extern int object_allocator_inited;
extern void * pvm_object_space_start;
extern void * pvm_object_space_end;


// ------------------------------------------------------------
// Private data
// ------------------------------------------------------------


// TODO for 64 bit mem we need more
#define N_PER_SIZE_ARENAS 12
static persistent_arena_t per_size_arena[N_PER_SIZE_ARENAS];

static persistent_arena_t curr_int_arena;     //< Integers
static persistent_arena_t curr_stack_arena;   //< Stack frames
static persistent_arena_t curr_static_arena;  //< Classes, code, etc
static persistent_arena_t curr_small_arena;   //< Less than 1K, but not one of the above
// TODO one for strings?


// ------------------------------------------------------------
// Prototypes for local code
// ------------------------------------------------------------


static void init_per_size_arena_flags( void );
static void alloc_find_arenas( void * _pvm_object_space_start, size_t size );
static void alloc_assert_is_arena( pvm_object_t a, void *start, void *end );
static persistent_arena_t *alloc_find_arena_by_flags( u_int32_t flags );

static int alloc_is_better_arena( persistent_arena_t *our, persistent_arena_t *src );

static void alloc_load_arena( persistent_arena_t *our, persistent_arena_t *src );
static void alloc_unload_arena( persistent_arena_t *our );

static void alloc_print_arena(persistent_arena_t *a);
static void alloc_print_arenas( void );

// ------------------------------------------------------------
// Loop for arenas
// ------------------------------------------------------------


void alloc_for_all_arenas( arena_iterator_t iter, void *arg )
{
    iter( &curr_int_arena, arg );
    iter( &curr_stack_arena, arg );
    iter( &curr_small_arena, arg );
    iter( &curr_static_arena, arg );

    int i;
    for( i = 0; i < N_PER_SIZE_ARENAS; i++ )
        iter( per_size_arena + i, arg );
}


// ------------------------------------------------------------
// Init
// ------------------------------------------------------------


void alloc_init_arenas( void * _pvm_object_space_start, size_t size )
{
    init_per_size_arena_flags();

    if( is_object_storage_initialized() )
    {
        // We have snapshot, just find out what's in memory
        alloc_find_arenas( _pvm_object_space_start, size );
        alloc_print_arenas();
        return;
    }

    // Memory is empty, start fron scratch.

    // Decide on arena sizes and create arena header objects in memory
}


void init_per_size_arena_flags( void )
{
    int flags = 0x400; // 1K bytes

    int i;
    for( i = 0; i < N_PER_SIZE_ARENAS; i++ )
    {
        per_size_arena[i].flags = flags;
        flags <<= 1;
    }

}



static void alloc_find_arenas( void * start, size_t size )
{
    void *end = start+size;
    void *curr = start;

    memset( per_size_arena, 0, sizeof(per_size_arena) );
    memset( &curr_int_arena, 0, sizeof(curr_int_arena) );
    memset( &curr_stack_arena, 0, sizeof(curr_stack_arena) );
    memset( &curr_static_arena, 0, sizeof(curr_static_arena) );
    memset( &curr_small_arena, 0, sizeof(curr_small_arena) );

    while( curr < end )
    {
        alloc_assert_is_arena( curr, start, end );
        
        pvm_object_t a = curr;
        persistent_arena_t *da = (persistent_arena_t *) &(a->da);

        curr = curr + da->size; // Step

        LOG_INFO_( 1, "Found arena size %zd, free %zd, flags %x", da->size, da->free, da->flags );

        // Now decide if we need this arena. If so - load it.

        persistent_arena_t * our = alloc_find_arena_by_flags( da->flags );
        if( our == 0 )
        {
            LOG_ERROR( 1, "Don't have arena for flags %x", da->flags );
            continue;
        }

        if( our->base == 0 )
            alloc_load_arena( our, da );
        else if( our->base == da->base )
            continue;
        else if( alloc_is_better_arena( our, da ) )
        {
            alloc_unload_arena( our );
            alloc_load_arena( our, da );
        }
    }
}





// ------------------------------------------------------------
// Sanity
// ------------------------------------------------------------

static void alloc_assert_is_arena( pvm_object_t a, void *start, void *end )
{
    assert( a->_flags & PHANTOM_OBJECT_STORAGE_FLAG_IS_ARENA );
    assert( (void *)a >= start );
    assert( (void *)a < end );

    persistent_arena_t *da = (persistent_arena_t *) &(a->da);

    assert( da->arena_start_marker == PVM_ARENA_START_MARKER );
    assert( da->free < da->size );
    assert( da->largest < da->size );

}

// ------------------------------------------------------------
// Choose arena
// ------------------------------------------------------------


/// Get pointer to one of our arena records corresponding to given flags
static persistent_arena_t *alloc_find_arena_by_flags( u_int32_t flags )
{
    if( flags & PHANTOM_ARENA_FOR_INT )    return &curr_int_arena;
    if( flags & PHANTOM_ARENA_FOR_STACK )  return &curr_stack_arena;
    if( flags & PHANTOM_ARENA_FOR_SMALL )  return &curr_small_arena;
    if( flags & PHANTOM_ARENA_FOR_STATIC ) return &curr_static_arena;

    int i;
    for( i = 0; i < N_PER_SIZE_ARENAS; i++ )
    {
        if( per_size_arena[i].flags == flags )
            return per_size_arena + i;
    }

    return 0;
}


static persistent_arena_t *alloc_find_arena_by_data_size( size_t size )
{
    if( size == sizeof(struct pvm_object_storage) + sizeof(struct data_area_4_int) ) 
        return &curr_int_arena;

    if( size < 1024 ) 
        return &curr_small_arena;

    size_t ss = size / 1024;
    int i;
    for( i = 0; i < N_PER_SIZE_ARENAS; i++ )
    {
        if( size & ~0x1 ) // bits above lower are non-zero - out of size for this range
        {
            ss >>= 1;
            continue;
        }

        if( 0 == per_size_arena[i].base )
        {
            LOG_ERROR( 0, "Attempt to alloc %d bytes from unloaded arena %d", size, i );
            continue; // Use next, bigger one
        }

        LOG_FLOW( 1, "Alloc %d bytes: found arena %d, flags %x", size, i, per_size_arena[i].flags );
        return per_size_arena+i;

    }

    LOG_ERROR( 0, "Attempt to alloc %d bytes: NO ARENA", size );
    return 0;
}

persistent_arena_t *find_arena(unsigned int size, unsigned int flags, bool saturated)
{

    if (flags & (PHANTOM_OBJECT_STORAGE_FLAG_IS_CALL_FRAME|PHANTOM_OBJECT_STORAGE_FLAG_IS_STACK_FRAME))
        return &curr_stack_arena; 

    if (flags & PHANTOM_OBJECT_STORAGE_FLAG_IS_INT)
        return &curr_int_arena;

    if (saturated)
        return &curr_static_arena;

    if (flags & (PHANTOM_OBJECT_STORAGE_FLAG_IS_CODE|PHANTOM_OBJECT_STORAGE_FLAG_IS_CLASS|PHANTOM_OBJECT_STORAGE_FLAG_IS_INTERFACE))
        return &curr_static_arena; //code|class|interface - never dies?

    return alloc_find_arena_by_data_size( size );

}

/*
persistent_arena_t * find_arena_by_address(void *memaddr)
{

}*/

static int alloc_is_better_arena( persistent_arena_t *our, persistent_arena_t *src )
{
    // TODO write me
    return 0; // never for now
}

// ------------------------------------------------------------
// Info
// ------------------------------------------------------------

static void alloc_print_arena(persistent_arena_t *a)
{
    LOG_INFO_( 1, "Arena flags %x, base %p, size %zd, free %zd, largest %zd", 
        a->flags, a->base, a->size, a->free, a->largest
         );
}

static void alloc_print_arenas( void )
{
    alloc_print_arena(  &curr_int_arena );
    alloc_print_arena( &curr_stack_arena );
    alloc_print_arena( &curr_small_arena );
    alloc_print_arena( &curr_static_arena );

    int i;
    for( i = 0; i < N_PER_SIZE_ARENAS; i++ )
        alloc_print_arena( per_size_arena+i );
}


// ------------------------------------------------------------
// Load/unload
// ------------------------------------------------------------

static void alloc_load_arena( persistent_arena_t *our, persistent_arena_t *src )
{
    // TODO need global spinlock to reload at run time
    *our = *src;
    our->owner = 0; // invalid? 
    //hal_mutex_init( &our->mutex, "MemArena" );
}

static void alloc_unload_arena( persistent_arena_t *our )
{
    // TODO need global spinlock to reload at run time
    //assert( 0 == hal_mutex_destroy(&our->mutex) );
}




//---------------------------------------------------------------------------
// Debug window - mem map
//---------------------------------------------------------------------------

#include <video/window.h>

//static rgba_t calc_object_pixel_color( int elem, int units_per_pixel );
static void paint_arena_memory_map(window_handle_t w, rect_t *r, persistent_arena_t * a );


/**
 * 
 * \brief Generic painter for any allocator using us.
 * 
 * Used in debug window.
 * 
 * \param[in] w Window to draw to
 * \param[in] r Rectangle to paint inside
 * 
**/

//#define ARENAS (N_PER_SIZE_ARENAS+4)
#define ARENAS N_PER_SIZE_ARENAS

static int vertical_start_for_arena[ARENAS];
static int vertical_pixels_per_arena[ARENAS];

void paint_object_memory_map(window_handle_t w, rect_t *r )
{
    int i;

    if(!object_allocator_inited) return;
    
    //pvm_memcheck();

    if(vertical_pixels_per_arena[0] == 0)
    {
        int y = 0;

        size_t omem_bytes = pvm_object_space_end - pvm_object_space_start;

        for( i = 0; i < ARENAS; i++) 
        {
            persistent_arena_t *ap;

            if( i < N_PER_SIZE_ARENAS )
                ap = per_size_arena+i;
            else
            {
                return; // TODO other arenas TODO put to array too?
            }
            

            size_t arena_bytes = ap->size;
            size_t percentage = arena_bytes * 100 / omem_bytes;

            int vpixels = (r->ysize * percentage) / 100;
            LOG_FLOW( 0, "omem=%uK, arena mem=%uK, mem %%=%u, ysize %d, ysize part = %d",
                omem_bytes/1024, arena_bytes/1024, percentage, r->ysize, vpixels
                );

            vertical_start_for_arena[i] = y;
            vertical_pixels_per_arena[i] = vpixels;
            y += vpixels;
        }
    }

    // now paint
    for( i = 0; i < ARENAS; i++) 
    {
        rect_t ar = *r;
        ar.y = vertical_start_for_arena[i];
        ar.ysize = vertical_pixels_per_arena[i];

        hal_mutex_lock( vm_alloc_mutex );  // TODO avoid Giant lock - must be per arena
        paint_arena_memory_map( w, &ar, per_size_arena+i );
        hal_mutex_unlock( vm_alloc_mutex );
    }

}

void paint_arena_memory_map(window_handle_t w, rect_t *r, persistent_arena_t * a )
{
    void * mem_start = a->base;
    void * mem_end = a->base + a->size;
    size_t mem_size = mem_end - mem_start;

    int pixels = r->xsize * r->ysize;
    int units_per_pixel =  1 + ((mem_size-1) / pixels);

    //int prev_object_crosses_pixel_and_not_free = 0;
    pvm_object_t curr = mem_start;

    void *this_pixel_start = mem_start;
    void *this_pixel_end = mem_start+units_per_pixel;

    LOG_FLOW( 0, "arena %d, start=%p, end=%p", a, mem_start, mem_end );

    int x, y;
    for( y = 0; y < r->ysize; y++ )
    {
        for( x = 0; x < r->xsize; x++ )
        {
            int cnt_used = 0;
            int cnt_free = 0;

            pvm_object_t next = ((void *)curr) + curr->_ah.exact_size;

            LOG_FLOW( 11, "pixel %d/%d, start=%p, end=%p", x, y, this_pixel_start, this_pixel_end );

            while( ((void *)next) < this_pixel_end )
            {
                assert( ((void *)curr) < this_pixel_end );

                if( curr->_ah.object_start_marker != PVM_OBJECT_START_MARKER ) 
                {
                    LOG_FLOW( 0, "object %p BROKEN", curr );
                    return;
                }

                assert( curr->_ah.exact_size != 0 );

                if( curr->_ah.alloc_flags & PVM_OBJECT_AH_ALLOCATOR_FLAG_ALLOCATED ) 
                    cnt_used++;
                else 
                    cnt_free++;

                curr = next;
                next = ((void *)curr) + curr->_ah.exact_size; 
            }

            w_draw_pixel( w, x + r->x, y + r->y, 
                (cnt_used == 0) ? COLOR_BLUE : ( (cnt_free == 0) ? COLOR_RED : COLOR_GREEN )
                );

            this_pixel_start += units_per_pixel;
            this_pixel_end   += units_per_pixel;

        }
    }

}
/*
static rgba_t calc_object_pixel_color( int elem, int units_per_pixel )
{
    vm_page *ep = vm_map_map + elem;

    int state = 0; // 0 = empty, 1 = partial, 2 = used
    int bits = 0;
    int do_io = 0;

    int i;
    for( i = 0; i < units_per_pixel; i++, ep++ )
    {
        if( 0 == ep->flag_phys_mem ) continue; // empty, no change
        state = 2; // full
        bits += 1;

        if( ep->flag_pager_io_busy ) 
        {
            do_io = 1;
            //lprintf("io %d ", elem+i);
        }
    }

    if(do_io) return COLOR_YELLOW;

    switch(state)
    {
        case 0: return COLOR_BLUE;
        
        case 1: 
        {
            //return COLOR_LIGHTGREEN;
            rgba_t c = COLOR_LIGHTGREEN;
            // lighter = less used
            c.g = 0xFF - (bits * 0xFF / (units_per_pixel * sizeof(map_elem_t) * 8));
            return c;
        }

        case 2: return COLOR_LIGHTRED;

        default: return COLOR_BLACK;
    }
}
*/



