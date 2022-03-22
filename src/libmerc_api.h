// libmerc_api.h

#ifndef LIBMERC_API_H
#define LIBMERC_API_H

#include "libmerc/libmerc.h"
#include <dlfcn.h>
#include <stdexcept>


struct libmerc_api {

    unsigned int libmerc_version = 0;  // represents API version

    libmerc_api(const char *lib_path) {
        if (bind(lib_path) != 0) {
            throw std::runtime_error("error: could not initialize libmerc_api");
        }
    }

    ~libmerc_api() {
        mercury_unbind(*this);
    }

    decltype(mercury_init)                                           *init = nullptr;
    decltype(mercury_finalize)                                       *finalize = nullptr;
    decltype(mercury_packet_processor_construct)                     *packet_processor_construct = nullptr;
    decltype(mercury_packet_processor_destruct)                      *packet_processor_destruct = nullptr;
    decltype(mercury_packet_processor_get_analysis_context)          *get_analysis_context = nullptr;
    decltype(mercury_packet_processor_get_analysis_context_linktype) *get_analysis_context_linktype = nullptr;
    decltype(analysis_context_get_fingerprint_type)                  *get_fingerprint_type = nullptr;
    decltype(analysis_context_get_fingerprint_status)                *get_fingerprint_status = nullptr;
    decltype(analysis_context_get_fingerprint_string)                *get_fingerprint_string = nullptr;
    decltype(analysis_context_get_server_name)                       *get_server_name = nullptr;
    decltype(analysis_context_get_alpns)                             *get_alpns = nullptr;
    decltype(analysis_context_get_user_agent)                        *get_user_agent = nullptr;
    decltype(analysis_context_get_process_info)                      *get_process_info = nullptr;
    decltype(analysis_context_get_malware_info)                      *get_malware_info = nullptr;
    decltype(mercury_write_stats_data)                               *write_stats_data = nullptr;
    decltype(register_printf_err_callback)                           *register_printf_err = nullptr;

    void *dl_handle = nullptr;

    int bind(const char *lib_path) {

        if ((dl_handle = dlopen(lib_path, RTLD_LAZY|RTLD_LOCAL)) == nullptr) {
            const char *dlerr = dlerror();
            fprintf(stderr, "mercury: failed to load %s: %s\n", lib_path, dlerr ? dlerr : "unknown error");
            return -1; // error
        } else {
            fprintf(stderr, "mercury: loading %s\n", lib_path);
        }

        // libmerc v1 API
        //
        init =                          (decltype(init))                          dlsym(dl_handle, "mercury_init");
        finalize =                      (decltype(finalize))                      dlsym(dl_handle, "mercury_finalize");
        packet_processor_construct =    (decltype(packet_processor_construct))    dlsym(dl_handle, "mercury_packet_processor_construct");
        packet_processor_destruct =     (decltype(packet_processor_destruct))     dlsym(dl_handle, "mercury_packet_processor_destruct");
        get_analysis_context =          (decltype(get_analysis_context))          dlsym(dl_handle, "mercury_packet_processor_get_analysis_context");
        get_fingerprint_type =          (decltype(get_fingerprint_type))          dlsym(dl_handle, "analysis_context_get_fingerprint_type");
        get_fingerprint_status =        (decltype(get_fingerprint_status))        dlsym(dl_handle, "analysis_context_get_fingerprint_status");
        get_fingerprint_string =        (decltype(get_fingerprint_string))        dlsym(dl_handle, "analysis_context_get_fingerprint_string");
        get_server_name =               (decltype(get_server_name))               dlsym(dl_handle, "analysis_context_get_server_name");
        get_process_info =              (decltype(get_process_info))              dlsym(dl_handle, "analysis_context_get_process_info");
        get_malware_info =              (decltype(get_malware_info))              dlsym(dl_handle, "analysis_context_get_malware_info");
        write_stats_data =              (decltype(write_stats_data))              dlsym(dl_handle, "mercury_write_stats_data");

        // verify that all v1 function symbols were found
        //
        if (init                          == nullptr ||
            finalize                      == nullptr ||
            packet_processor_construct    == nullptr ||
            packet_processor_destruct     == nullptr ||
            get_analysis_context          == nullptr ||
            get_fingerprint_type          == nullptr ||
            get_fingerprint_status        == nullptr ||
            get_fingerprint_string        == nullptr ||
            get_process_info              == nullptr ||
            get_malware_info              == nullptr ||
            write_stats_data              == nullptr) {
            fprintf(stderr, "error: could not initialize one or more libmerc v1 function pointers\n");
            return -1;
        }
        libmerc_version = 1;

        // libmerc v2 API
        //
        register_printf_err =        (decltype(register_printf_err))        dlsym(dl_handle, "register_printf_err_callback");

        // check to see if all v2 function symbols were found
        //
        if (register_printf_err           == nullptr) {
            fprintf(stderr, "note: could not initialize one or more libmerc v2 function pointers\n");
        } else {
            libmerc_version = 2;
        }

        // libmerc v3 API
        //
        get_analysis_context_linktype = (decltype(get_analysis_context_linktype)) dlsym(dl_handle, "mercury_packet_processor_get_analysis_context_linktype");
        get_alpns =                     (decltype(get_alpns))                     dlsym(dl_handle, "analysis_context_get_alpns");
        get_user_agent =                (decltype(get_user_agent))                dlsym(dl_handle, "analysis_context_get_user_agent");

        // verify that all v3 function symbols were found
        //
        if (get_analysis_context_linktype == nullptr ||
            get_user_agent                == nullptr ||
            get_alpns                     == nullptr) {
            fprintf(stderr, "note: could not initialize one or more libmerc v3 function pointers\n");
        } else {
            libmerc_version = 3;
        }

        fprintf(stderr, "libmerc api version %u found\n", libmerc_version);
        fprintf(stderr, "mercury_bind() succeeded with handle %p\n", dl_handle);

        return 0; // success
    }

    void mercury_unbind(struct libmerc_api &libmerc_api) {
        dlclose(libmerc_api.dl_handle);
        libmerc_api.dl_handle = nullptr;
    }

};

#endif // LIBMERC_API_H
