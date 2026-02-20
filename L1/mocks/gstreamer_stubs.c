/*
 * GStreamer Type System Stubs for Westeros Sink Testing
 * Provides minimal implementations to allow GStreamer function calls in tests
 */

#include <glib.h>
#include <gst/gst.h>
#include <gst/base/gstbasesink.h>
#include <string.h>
#include <stdio.h>

void __wrap_gst_base_sink_set_sync(void *sink, gboolean sync)
{
    (void)sink;
    (void)sync;
}

void __wrap_gst_base_sink_set_async_enabled(void *sink, gboolean async)
{
    (void)sink;
    (void)async;
}

gboolean __wrap_gst_base_sink_is_async_enabled(void *sink)
{
    (void)sink;
    return TRUE;
}

GstMemory* __wrap_gst_buffer_peek_memory(GstBuffer *buffer, guint idx)
{
    (void)buffer;
    (void)idx;
    return NULL;
}

void __wrap_gst_element_class_set_static_metadata(GstElementClass *klass,
                                                   const gchar *longname,
                                                   const gchar *classification,
                                                   const gchar *description,
                                                   const gchar *author)
{
    (void)klass;
    (void)longname;
    (void)classification;
    (void)description;
    (void)author;
}

void __wrap_g_object_class_install_property(GObjectClass *oclass, guint property_id, GParamSpec *pspec)
{
    (void)oclass;
    (void)property_id;
    (void)pspec;
}

void __wrap_g_object_class_override_property(GObjectClass *oclass, guint property_id, const gchar *name)
{
    (void)oclass;
    (void)property_id;
    (void)name;
}

guint __wrap_g_signal_new(const gchar *signal_name,
                          GType itype,
                          GSignalFlags signal_flags,
                          guint class_offset,
                          GSignalAccumulator accumulator,
                          gpointer accu_data,
                          GSignalCMarshaller c_marshaller,
                          GType return_type,
                          guint n_params,
                          ...)
{
    (void)signal_name;
    (void)itype;
    (void)signal_flags;
    (void)class_offset;
    (void)accumulator;
    (void)accu_data;
    (void)c_marshaller;
    (void)return_type;
    (void)n_params;
    return 1;
}

GstCaps* __wrap_gst_caps_from_string(const gchar *string)
{
    (void)string;
    return NULL;
}

GstPadTemplate* __wrap_gst_pad_template_new(const gchar *name_template,
                                            GstPadDirection direction,
                                            GstPadPresence presence,
                                            GstCaps *caps)
{
    (void)name_template;
    (void)direction;
    (void)presence;
    (void)caps;
    return NULL;
}

GstPadTemplate* __wrap_gst_static_pad_template_get(GstStaticPadTemplate *pad_template)
{
    (void)pad_template;
    return NULL;
}

void __wrap_gst_element_class_add_pad_template(GstElementClass *klass, GstPadTemplate *templ)
{
    (void)klass;
    (void)templ;
}

GList* __wrap_gst_type_find_factory_get_list(void)
{
    return NULL;
}

void __wrap_gst_value_set_caps(GValue *value, const GstCaps *caps)
{
    (void)value;
    (void)caps;
}

GstCaps* __wrap_gst_caps_merge(GstCaps *caps1, GstCaps *caps2)
{
    (void)caps1;
    (void)caps2;
    return NULL;
}

void __wrap_gst_pad_set_event_function(GstPad *pad, GstPadEventFunction event)
{
    (void)pad;
    (void)event;
}

void __wrap_gst_pad_set_link_function(GstPad *pad, GstPadLinkFunction link)
{
    (void)pad;
    (void)link;
}

void __wrap_gst_pad_set_unlink_function(GstPad *pad, GstPadUnlinkFunction unlink)
{
    (void)pad;
    (void)unlink;
}

void __wrap_gst_pad_set_query_function(GstPad *pad, GstPadQueryFunction query)
{
    (void)pad;
    (void)query;
}

GstPadEventFunction __wrap_gst_pad_get_eventfunc(GstPad *pad)
{
    (void)pad;
    return NULL;
}

GstPadQueryFunction __wrap_gst_pad_get_queryfunc(GstPad *pad)
{
    (void)pad;
    return NULL;
}

GObject* __wrap_g_object_new(GType object_type, const gchar *first_property_name, ...)
{
    (void)object_type;
    (void)first_property_name;
    return NULL;
}

GTypeInstance* __wrap_g_type_check_instance_cast(GTypeInstance *instance, GType iface_type)
{
    (void)iface_type;
    return instance;
}

gboolean __wrap_g_type_check_instance_is_a(GTypeInstance *instance, GType iface_type)
{
    (void)instance;
    (void)iface_type;
    return TRUE;
}

typedef struct {
    gchar *name;
    GList *children;  /* For bins */
    gint32 refcount;
} MockBinData;

GstElement* gst_element_factory_make(const gchar *factoryname, const gchar *name)
{
    MockBinData *elem;
    gchar element_name[256];
    
    if (!name) {
        snprintf(element_name, sizeof(element_name), "%s", factoryname ? factoryname : "element");
    } else {
        snprintf(element_name, sizeof(element_name), "%s", name);
    }
    
    elem = (MockBinData *)g_malloc0(sizeof(MockBinData));
    if (!elem) return NULL;
    
    elem->name = g_strdup(element_name);
    elem->refcount = 1;
    elem->children = NULL;
    
    return (GstElement *)elem;
}

GstElement* gst_bin_new(const gchar *name)
{
    MockBinData *bin;
    gchar bin_name[256];
    
    if (!name) {
        snprintf(bin_name, sizeof(bin_name), "bin");
    } else {
        snprintf(bin_name, sizeof(bin_name), "%s", name);
    }
    
    bin = (MockBinData *)g_malloc0(sizeof(MockBinData));
    if (!bin) return NULL;
    
    bin->name = g_strdup(bin_name);
    bin->refcount = 1;
    bin->children = NULL;
    
    return (GstElement *)bin;
}

gboolean gst_bin_add(GstBin *bin, GstElement *element)
{
    MockBinData *mock_bin = (MockBinData *)bin;
    MockBinData *mock_elem = (MockBinData *)element;
    
    if (mock_bin && mock_elem) {
        mock_bin->children = g_list_append(mock_bin->children, mock_elem);
        return TRUE;
    }
    return FALSE;
}

void gst_bin_add_many(GstBin *bin, GstElement *element1, ...)
{
    va_list args;
    GstElement *elem = element1;
    
    va_start(args, element1);
    while (elem) {
        gst_bin_add(bin, elem);
        elem = va_arg(args, GstElement *);
    }
    va_end(args);
}

gboolean gst_element_link(GstElement *src, GstElement *dest)
{
    (void)src;
    (void)dest;
    return TRUE; 
}

void gst_element_unlink(GstElement *src, GstElement *dest)
{
    (void)src;
    (void)dest;
}

GstStateChangeReturn gst_element_set_state(GstElement *element, GstState state)
{
    (void)element;
    (void)state;
    return GST_STATE_CHANGE_SUCCESS;
}

void gst_object_unref(gpointer object)
{
    MockBinData *elem = (MockBinData *)object;
    if (!elem) return;
    
    elem->refcount--;
    if (elem->refcount <= 0) {
        g_list_free(elem->children);
        
        if (elem->name) g_free(elem->name);
        g_free(elem);
    }
}

GstIterator* gst_bin_iterate_sorted(GstBin *bin)
{
    (void)bin;  
    return NULL;  
}

GstIterator* gst_element_iterate_sink_pads(GstElement *element)
{
    (void)element; 
    return NULL;  
}

GstIterator* gst_element_iterate_src_pads(GstElement *element)
{
    (void)element;  
    return NULL;  
}

GstPad* gst_pad_get_peer(GstPad *pad)
{
    (void)pad;
    return NULL;
}

GstElement* gst_pad_get_parent_element(GstPad *pad)
{
    (void)pad;
    return NULL;
}

GstIteratorResult gst_iterator_next(GstIterator *it, GValue *elem)
{
    (void)elem;
    if (!it) return GST_ITERATOR_DONE;
    return GST_ITERATOR_DONE;
}

void gst_iterator_free(GstIterator *it)
{
    if (it) g_free(it);
}

#undef gst_element_get_name
const gchar* gst_element_get_name(GstElement *element)
{
    MockBinData *mock_elem = (MockBinData *)element;
    if (mock_elem && mock_elem->name) {
        return mock_elem->name;
    }
    return "unknown";
}

void gst_init(int *argc, char **argv[])
{
    (void)argc;
    (void)argv;
}

void gst_deinit(void)
{
}

void gst_value_set_caps(GValue *value, const GstCaps *caps)
{
    (void)value;
    (void)caps;
}

GstCaps* gst_caps_new_empty(void)
{
    return NULL;
}

GstCaps* __wrap_gst_caps_new_simple(const gchar *media_type, ...)
{
    (void)media_type;
    return (GstCaps *)g_malloc0(sizeof(void *));  
}

void __wrap_gst_caps_unref(GstCaps *caps)
{
    if (caps) {
        g_free(caps);  
    }
}

GstCaps* gst_caps_merge(GstCaps *caps1, GstCaps *caps2)
{
    (void)caps1;
    (void)caps2;
    return NULL;
}