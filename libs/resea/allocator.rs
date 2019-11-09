use crate::PAGE_SIZE;
use crate::message::Page;
use crate::std::vec::Vec;
use crate::lazy_static::LazyStatic;
use core::mem::MaybeUninit;
use core::cell::RefCell;
use linked_list_allocator::LockedHeap;

#[global_allocator]
static ALLOCATOR: LockedHeap = LockedHeap::empty();
static VALLOC_ALLOCATOR: LazyStatic<RefCell<PageAllocator>> = LazyStatic::new();

extern "C" {
    static __heap: u8;
    static __heap_end: u8;
    static __valloc: u8;
    static __valloc_end: u8;
}

pub struct AllocatedPage {
    pub addr: usize,
    pub num_pages: usize,
}

impl AllocatedPage {
    pub fn as_mut_ptr(&mut self) -> *mut u8 {
        self.addr as *mut u8
    }

    pub fn as_page_payload(&self) -> Page {
        Page::new(self.addr, self.num_pages)
    }
}

pub struct FreeArea {
    addr: usize,
    num_pages: usize,
}

pub struct PageAllocator {
    free_area: Vec<FreeArea>,
}

impl PageAllocator {
    pub fn new(free_area_start: usize, free_area_size: usize) -> PageAllocator {
        let free_area = FreeArea {
            addr: free_area_start,
            num_pages: free_area_size / PAGE_SIZE,
        };

        PageAllocator {
            free_area: vec![free_area],
        }
    }

    pub fn allocate(&mut self, num_pages: usize) -> AllocatedPage {
        while let Some(free_area) = self.free_area.pop() {
            if num_pages <= free_area.num_pages {
                let remaining_pages = free_area.num_pages - num_pages;
                if remaining_pages > 0 {
                    self.free_area.push(FreeArea {
                        addr: free_area.addr + PAGE_SIZE * num_pages,
                        num_pages: remaining_pages,
                    });
                }
                return AllocatedPage {
                    addr: free_area.addr,
                    num_pages,
                };
            }
        };

        panic!("out of memory");
    }
}

pub fn valloc(num_pages: usize) -> usize {
    VALLOC_ALLOCATOR.borrow_mut().allocate(num_pages).addr
}

pub fn init() {
    unsafe {
        let heap_start = &__heap as *const u8 as usize;
        let heap_end = &__heap_end as *const u8 as usize;
        ALLOCATOR.lock().init(heap_start, heap_end - heap_start);

        let valloc_start = &__valloc as *const u8 as usize;
        let valloc_end = &__valloc_end as *const u8 as usize;
        VALLOC_ALLOCATOR.init(
            RefCell::new(PageAllocator::new(valloc_start, valloc_end - valloc_start)));
    }
}
