/**
 * @name Use of gmtime without proper lock
 * @description In the absence of the safer gmtime_r function, any call to
 * gmtime should be protected by a lock
 * Based on LGTM's built-in query 'cpp/potentially-dangerous-function'
 * (https://lgtm.com/rules/2154840805) after discussion here:
 * https://discuss.lgtm.com/t/suppressions-in-c-code/1525
 *
 * @kind problem
 * @problem.severity error
 * @precision high
 * @id cpp/call-to-gmtime-without-lock
 * @tags reliability
 *       security
 *       external/cwe/cwe-676
 */
import cpp
import semmle.code.cpp.controlflow.Dominance

predicate isLockedUseOfFunction(FunctionCall call){
	exists (FunctionCall lockCall |
		lockCall.getTarget().getQualifiedName() = "g_mutex_lock"
		and dominates(lockCall, call)
	)
}

from FunctionCall call
where call.getTarget().getName() = "gmtime"
and isLockedUseOfFunction(call)

select call, "Call to gmtime is not protected by a lock"
