#include "psgi.h" 

extern struct uwsgi_server uwsgi;
struct uwsgi_perl uperl;



XS(XS_input_seek) {

        dXSARGS;

        psgi_check_args(1);
        XSRETURN(0);
}

XS(XS_error) {
	dXSARGS;
	struct wsgi_request *wsgi_req = current_wsgi_req();
	struct uwsgi_app *wi = &uwsgi_apps[wsgi_req->app_id];

        psgi_check_args(0);

        ST(0) = sv_bless(newRV(sv_newmortal()), ((HV **)wi->error)[wsgi_req->async_id]);
        XSRETURN(1);
}

XS(XS_input) {

        dXSARGS;
	struct wsgi_request *wsgi_req = current_wsgi_req();
	struct uwsgi_app *wi = &uwsgi_apps[wsgi_req->app_id];
        psgi_check_args(0);

        ST(0) = sv_bless(newRV(sv_newmortal()), ((HV **)wi->input)[wsgi_req->async_id]);
        XSRETURN(1);
}

XS(XS_stream)
{
    dXSARGS;
    struct wsgi_request *wsgi_req = current_wsgi_req();
    struct uwsgi_app *wi = &uwsgi_apps[wsgi_req->app_id];

    psgi_check_args(1);

    AV *response = (AV* ) SvREFCNT_inc(SvRV(ST(0))) ;

	if (av_len(response) == 2) {
		while (psgi_response(wsgi_req, response) != UWSGI_OK);
	}
	else if (av_len(response) == 1) {
		while (psgi_response(wsgi_req, response) != UWSGI_OK);

		SvREFCNT_dec(response);
                ST(0) = sv_bless(newRV(sv_newmortal()), ((HV **)wi->stream)[wsgi_req->async_id]);
                XSRETURN(1);
	}
	else {
		uwsgi_log("invalid PSGI response: array size %d\n", av_len(response));
	}

	SvREFCNT_dec(response);
	XSRETURN(0);

}


XS(XS_input_read) {

        dXSARGS;
        struct wsgi_request *wsgi_req = current_wsgi_req();
        int fd = -1;
        char *tmp_buf;
        ssize_t bytes = 0, len;
        size_t remains;
        SV *read_buf;

        psgi_check_args(3);


        read_buf = ST(1);
        len = SvIV(ST(2));

        // return empty string if no post_cl or pos >= post_cl
        if (!wsgi_req->post_cl || (size_t) wsgi_req->post_pos >= wsgi_req->post_cl) {
                sv_setpvn(read_buf, "", 0);
                goto ret;
        }

        if (wsgi_req->body_as_file) {
                fd = fileno((FILE *)wsgi_req->async_post);
        }
        else if (uwsgi.post_buffering > 0) {
                fd = -1;
                if (wsgi_req->post_cl > (size_t) uwsgi.post_buffering) {
                        fd = fileno((FILE *)wsgi_req->async_post);
                }
        }
        else {
                fd = wsgi_req->poll.fd;
        }
        // return the whole input
        if (len <= 0) {
                remains = wsgi_req->post_cl;
        }
        else {
                remains = len ;
        }

        if (remains + wsgi_req->post_pos > wsgi_req->post_cl) {
                remains = wsgi_req->post_cl - wsgi_req->post_pos;
        }

        if (remains <= 0) {
                sv_setpvn(read_buf, "", 0);
                goto ret;
        }

        // data in memory ?
        if (fd == -1) {
                sv_setpvn(read_buf, wsgi_req->post_buffering_buf, remains);
                bytes = remains;
                wsgi_req->post_pos += remains;

        }

        tmp_buf = uwsgi_malloc(remains);

        if (uwsgi_waitfd(fd, uwsgi.shared->options[UWSGI_OPTION_SOCKET_TIMEOUT]) <= 0) {
                free(tmp_buf);
                croak("error waiting for wsgi.input data");
                goto ret;
        }

        bytes = read(fd, tmp_buf, remains);
        if (bytes < 0) {
                free(tmp_buf);
                croak("error reading wsgi.input data");
                goto ret;
        }

        wsgi_req->post_pos += bytes;
        sv_setpvn(read_buf, tmp_buf, bytes);

        free(tmp_buf);

ret:
        XSRETURN_IV(bytes);
}


XS(XS_streaming_close) {

        dXSARGS;
        psgi_check_args(0);

        XSRETURN(0);
}

XS(XS_streaming_write) {

        dXSARGS;
        struct wsgi_request *wsgi_req = current_wsgi_req();
        STRLEN blen;
        char *body;

        psgi_check_args(2);

        body = SvPV(ST(1), blen);

        wsgi_req->response_size += wsgi_req->socket->proto_write(wsgi_req, body, blen);

        XSRETURN(0);
}

XS(XS_error_print) {

	dXSARGS;
        STRLEN blen;
        char *body;

        psgi_check_args(1);

	if (items > 1) {
        	body = SvPV(ST(1), blen);
		uwsgi_log("%.*s", blen, body);
	}

        XSRETURN(0);
}

/* automatically generated */

EXTERN_C void xs_init (pTHX);

EXTERN_C void boot_DynaLoader (pTHX_ CV* cv);

        EXTERN_C void
xs_init(pTHX)
{
        char *file = __FILE__;
        dXSUB_SYS;

        /* DynaLoader is a special case */
        newXS("DynaLoader::boot_DynaLoader", boot_DynaLoader, file);

        newXS("uwsgi::input::new", XS_input, "uwsgi::input");
        newXS("uwsgi::input::read", XS_input_read, "uwsgi::input");
        newXS("uwsgi::input::seek", XS_input_seek, "uwsgi::input");

        uperl.tmp_input_stash[uperl.tmp_current_i] = gv_stashpv("uwsgi::input", 0);

        newXS("uwsgi::error::new", XS_error, "uwsgi::error");
        newXS("uwsgi::error::print", XS_error_print, "uwsgi::print");
        uperl.tmp_error_stash[uperl.tmp_current_i] = gv_stashpv("uwsgi::error", 0);

        uperl.tmp_stream_responder[uperl.tmp_current_i] = newXS("uwsgi::stream", XS_stream, "uwsgi");

        newXS("uwsgi::streaming::write", XS_streaming_write, "uwsgi::streaming");
        newXS("uwsgi::streaming::close", XS_streaming_close, "uwsgi::streaming");

        uperl.tmp_streaming_stash[uperl.tmp_current_i] = gv_stashpv("uwsgi::streaming", 0);

#ifdef UWSGI_EMBEDDED
        init_perl_embedded_module();
#endif

}

/* end of automagically generated part */

PerlInterpreter *uwsgi_perl_new_interpreter(void) {

	PerlInterpreter *pi = perl_alloc();
        if (!pi) {
                uwsgi_log("unable to allocate perl interpreter\n");
                return NULL;
        }

	PERL_SET_CONTEXT(pi);

        PL_perl_destruct_level = 2;
        PL_origalen = 1;
        perl_construct(pi);
	// over-engeneering
        PL_origalen = 1;

	return pi;
}

static void uwsgi_perl_free_stashes(void) {
        free(uperl.tmp_streaming_stash);
        free(uperl.tmp_input_stash);
        free(uperl.tmp_error_stash);
        free(uperl.tmp_stream_responder);
}

int init_psgi_app(struct wsgi_request *wsgi_req, char *app, uint16_t app_len, PerlInterpreter **interpreters) {

	struct stat st;
	int i;
	SV **callables;

	char *app_name = uwsgi_concat2n(app, app_len, "", 0);

	// prepare for $0
	uperl.embedding[1] = app_name;
		
	int fd = open(app_name, O_RDONLY);
	if (fd < 0) {
		uwsgi_error_open(app_name);
		goto clear2;
	}

	if (fstat(fd, &st)) {
		uwsgi_error("fstat()");
		close(fd);
		goto clear2;
	}

	char *buf = uwsgi_calloc(st.st_size+1);
	if (read(fd, buf, st.st_size) != st.st_size) {
		uwsgi_error("read()");
		close(fd);
		free(buf);
		goto clear2;
	}

	close(fd);

	// the first (default) app, should always be loaded in the main interpreter
	if (interpreters == NULL) {
		if (uwsgi_apps_cnt) {
			interpreters = uwsgi_calloc(sizeof(PerlInterpreter *) * uwsgi.threads);
			interpreters[0] = uwsgi_perl_new_interpreter();
			if (!interpreters[0]) {
				uwsgi_log("unable to create new perl interpreter\n");
				free(interpreters);
				goto clear2;
			}
		}
		else {
			interpreters = uperl.main;
		}		
	}

	if (!interpreters) {
		goto clear2;
	}


	callables = uwsgi_calloc(sizeof(SV *) * uwsgi.threads);
	uperl.tmp_streaming_stash = uwsgi_calloc(sizeof(HV *) * uwsgi.threads);
	uperl.tmp_input_stash = uwsgi_calloc(sizeof(HV *) * uwsgi.threads);
	uperl.tmp_error_stash = uwsgi_calloc(sizeof(HV *) * uwsgi.threads);
	uperl.tmp_stream_responder = uwsgi_calloc(sizeof(CV *) * uwsgi.threads);

	for(i=0;i<uwsgi.threads;i++) {

		if (i > 0 && interpreters != uperl.main) {
		
			interpreters[i] = uwsgi_perl_new_interpreter();
			if (!interpreters[i]) {
				uwsgi_log("unable to create new perl interpreter\n");
				// what to do here ? i hope no-one will use threads with dynamic apps...but clear the whole stuff...
				free(callables);
				uwsgi_perl_free_stashes();
				while(i>=0) {
					perl_destruct(interpreters[i]);	
					perl_free(interpreters[i]);
					goto clear2;
				}
			}
		}

		PERL_SET_CONTEXT(interpreters[i]);

		uperl.tmp_current_i = i;


		if (uperl.locallib) {
                        uwsgi_log("using %s as local::lib directory\n", uperl.locallib);
                        uperl.embedding[1] = uwsgi_concat2("-Mlocal::lib=", uperl.locallib);
                        uperl.embedding[2] = app_name;
                        if (perl_parse(interpreters[i], xs_init, 3, uperl.embedding, NULL)) {
				// what to do here ? i hope no-one will use threads with dynamic apps... but clear the whole stuff...
				free(uperl.embedding[1]);
				uperl.embedding[1] = app_name;
				free(callables);
				uwsgi_perl_free_stashes();
				goto clear;
                        }
			free(uperl.embedding[1]);
			uperl.embedding[1] = app_name;
                }
		else {
			if (perl_parse(interpreters[i], xs_init, 2, uperl.embedding, NULL)) {
				// what to do here ? i hope no-one will use threads with dynamic apps... but clear the whole stuff...
				free(callables);
				uwsgi_perl_free_stashes();
				goto clear;
        		}
		}

		perl_eval_pv("use IO::Handle;", 0);
		perl_eval_pv("use IO::File;", 0);

		SV *dollar_zero = get_sv("0", GV_ADD);
		sv_setsv(dollar_zero, newSVpv(app, app_len));

		callables[i] = perl_eval_pv(uwsgi_concat4("#line 1 ", app_name, "\n", buf), 0);
		if (!callables[i]) {
			uwsgi_log("unable to find PSGI function entry point.\n");
			// what to do here ? i hope no-one will use threads with dynamic apps...
			free(callables);
			uwsgi_perl_free_stashes();
                	goto clear;
		}

		PERL_SET_CONTEXT(interpreters[0]);
	}

	free(buf);

	if(SvTRUE(ERRSV)) {
        	uwsgi_log("%s\n", SvPV_nolen(ERRSV));
		free(callables);
		uwsgi_perl_free_stashes();
		goto clear;
        }

	int id = uwsgi_apps_cnt;
	struct uwsgi_app *wi = NULL;

	if (wsgi_req) {
		// we need a copy of app_id
		wi = uwsgi_add_app(id, 5, uwsgi_concat2n(wsgi_req->appid, wsgi_req->appid_len, "", 0), wsgi_req->appid_len, interpreters, callables);
	}
	else {
		wi = uwsgi_add_app(id, 5, "", 0, interpreters, callables);
	}

        uwsgi_log("PSGI app %d (%s) loaded at %p (interpreter %p)\n", id, app_name, callables[0], interpreters[0]);
	free(app_name);

	// copy global data to app-specific areas
	wi->stream = uperl.tmp_streaming_stash;
	wi->input = uperl.tmp_input_stash;
	wi->error = uperl.tmp_error_stash;
	wi->responder0 = uperl.tmp_stream_responder;

	// check if we need to emulate fork() COW
        if (uwsgi.mywid == 0) {
                for(i=1;i<=uwsgi.numproc;i++) {
                        memcpy(&uwsgi.workers[i].apps[id], &uwsgi.workers[0].apps[id], sizeof(struct uwsgi_app));
                        uwsgi.workers[i].apps_cnt = uwsgi_apps_cnt;
                }
        }


	// restore context if required
	if (interpreters != uperl.main) {
		PERL_SET_CONTEXT(uperl.main[0]);
	}

	return id;

clear:
	if (interpreters != uperl.main) {
		for(i=0;i<uwsgi.threads;i++) {
			perl_destruct(interpreters[i]);
			perl_free(interpreters[i]);
		}
		free(interpreters);
	}

	PERL_SET_CONTEXT(uperl.main[0]);
clear2:
	free(app_name);
       	return -1; 
}

void uwsgi_psgi_app() {

        if (uperl.psgi) {
		//load app in the main interpreter list
		init_psgi_app(NULL, uperl.psgi, strlen(uperl.psgi), uperl.main);
        }

}

