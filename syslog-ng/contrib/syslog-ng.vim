" Vim syntax file
" Language:	syslog-ng: syslog-ng main configuration file (1.5.5a)
" Maintainer:	--
" Last change:	2001 Apr 13
" URL:		--
" syslog-ng's home:	http://www.balabit.hu

" Remove any old syntax stuff hanging around
syn clear
syn case match
set iskeyword=a-z,A-Z,48-57,_,-,.

syn keyword sysngStatement	source destination filter log options
syn match sysngComment		"#.*$"
syn match sysngString		+"[^"]*"+
syn match sysngOctNumber	"\<0\o\+\>"
syn match sysngDecNumber	"\<\d\+\>"
syn match sysngHexNumber	"\<0x\x\+\>"
syn keyword sysngBool		yes no on off
syn match sysngIdentifier	"\<[sdf]_\+\>"

syn keyword sysngDriver		internal remote_control
syn keyword sysngDriver		file fifo pipe door
syn keyword sysngDriver		udp tcp
syn keyword sysngDriver		sun_stream sun_streams sun-stream sun-streams
syn keyword sysngDriver		unix_dgram unix_stream unix-dgram unix-stream
syn keyword sysngDriver		usertty program

syn keyword sysngFilter		not and or .. level priority facility
syn keyword sysngFilter		program host match DEFAULT

if !exists("did_sysng_syntax_inits")
    let did_sysng_syntax_inits = 1

    hi link sysngStatement	Statement
    hi link sysngComment	Comment
    hi link sysngString		String
    hi link sysngOctNumber	Number
    hi link sysngDecNumber	Number
    hi link sysngHexNumber	Number
    hi link sysngBool		Constant
    hi link sysngIdentifier	Identifier

    hi link sysngDriver		Type
    hi link sysngFilter		Operator
endif

let b:current_syntax = "syslog-ng"
