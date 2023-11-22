#!/bin/sh

if test -d ./.git ; then \
	B=`git branch | sed -n -e 's/^\* \(.*\)/\1/p'`; \
	M=`git status --porcelain` ; \
if test "$$M" != "" ; then \
	M="M" ; \
fi ; \
if test "$$B" = "master" ; then \
	V="$$M" ; \
elif test "$$B" = "develop" ; then \
	V=`git rev-list HEAD | wc -l | xargs` ; \
	V="$${V}$$M" ; \
else \
	V=`git rev-list HEAD | wc -l | xargs` ; \
	V="$${V}$$M ($$B)" ; \
fi ; \
	H=`test -f ./code_revision.cpp && head -n 1 code_revision.cpp`; \
if test "/* $$V */" != "$$H" ; then \
	( \
	echo "/* $$V */" ;\
	echo "/* This file is automatically regenerated on each build. Do not edit it. */" ;\
	echo "#include \"nzbget.h\"" ;\
	echo "const char* code_revision(void)" ;\
	echo "{" ;\
	echo "const char* revision = \"$$V\";" ;\
	echo "return revision;" ;\
	echo "}" ;\
) > code_revision.cpp ; \
fi \
elif test -f ./code_revision.cpp ; then \
	test "ok, reuse existing file"; \
else \
	( \
	echo "/*  */" ;\
	echo "/* This file is automatically regenerated on each build. Do not edit it. */" ;\
	echo "#include \"nzbget.h\"" ;\
	echo "const char* code_revision(void)" ;\
	echo "{" ;\
	echo "const char* revision = \"\";" ;\
	echo "return revision;" ;\
	echo "}" ;\
	) > code_revision.cpp ; \
fi
