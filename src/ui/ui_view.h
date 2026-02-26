#pragma once

#include "base/core.h"
#include "base/string.h"
#include "ui/ui.h"

istruct (ViewInstance); 

istruct (ViewType) {
    String (*get_icon)  (ViewInstance*);
    String (*get_title) (ViewInstance*);
    UiBox *(*build)     (ViewInstance*);
};

typedef U64 ViewId;

istruct (ViewInstance) {
    ViewId id;
    ViewType *type;
    Void *data;
};

array_typedef(ViewInstance*, ViewInstance);
array_typedef(ViewType, ViewType);

istruct (ViewStore) {
    Mem *mem;
    ViewId next_id;
    ArrayViewType types;
    ArrayViewInstance instances;
};

ViewStore    *ui_view_store_new       (Mem *);
Void          ui_view_type_add        (ViewStore *, ViewType);
ViewInstance *ui_view_instance_get    (ViewStore *, ViewId);
ViewInstance *ui_view_instance_new    (ViewStore *, ViewType *);
Void          ui_view_instance_remove (ViewStore *, ViewInstance *);
