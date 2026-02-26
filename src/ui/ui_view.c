#include "ui/ui_view.h"

ViewStore *ui_view_store_new (Mem *mem) {
    ViewStore *store = mem_new(mem, ViewStore);
    store->mem = mem;
    array_init(&store->instances, mem);
    array_init(&store->types, mem);
    return store;
}

ViewInstance *ui_view_instance_get (ViewStore *store, ViewId id) {
    array_iter (it, &store->instances) {
        if (it->id == id) return it;
    }

    return 0;
}

ViewInstance *ui_view_instance_new (ViewStore *store, ViewType *type) {
    ViewInstance *instance = mem_new(store->mem, ViewInstance);
    instance->id = store->next_id++;
    instance->type = type;
    return instance;
}

Void ui_view_instance_remove (ViewStore *store, ViewInstance *instance) {
    array_find_remove_fast(&store->instances, IT == instance);
}

Void ui_view_type_add (ViewStore *store, ViewType type) {
    array_push(&store->types, type);
}
