/*
 * Copyright (c) 2010-2018 Balabit
 * Copyright (c) 2010-2015 Balázs Scheidler <balazs.scheidler@balabit.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 */

#ifndef __TEST_PATTERNDB_H__
#define __TEST_PATTERNDB_H__

#define MYHOST "MYHOST"
#define MYPID "999"

#define pdb_conflicting_rules_with_different_parsers "<patterndb version='4' pub_date='2010-02-22'>\
 <ruleset name='testset' id='1'> \
  <patterns>\
   <pattern>prog1</pattern>\
   <pattern>prog2</pattern>\
  </patterns>\
  <rules>\
    <!-- different parsers at the same location -->\
    <rule provider='test' id='11' class='short'>\
     <patterns>\
      <pattern>pattern @ESTRING:foo1: @</pattern>\
     </patterns>\
    </rule>\
    <rule provider='test' id='12' class='long'>\
     <patterns>\
      <pattern>pattern @ESTRING:foo2: @tail</pattern>\
     </patterns>\
    </rule>\
  </rules>\
 </ruleset>\
</patterndb>"

#define pdb_conflicting_rules_with_the_same_parsers "<patterndb version='4' pub_date='2010-02-22'>\
 <ruleset name='testset' id='1'>\
  <patterns>\
   <pattern>prog1</pattern>\
   <pattern>prog2</pattern>\
  </patterns>\
  <rules>\
    <!-- different parsers at the same location -->\
    <rule provider='test' id='11' class='short'>\
     <patterns>\
      <pattern>pattern @ESTRING:foo: @</pattern>\
     </patterns>\
    </rule>\
    <rule provider='test' id='12' class='long'>\
     <patterns>\
      <pattern>pattern @ESTRING:foo: @tail</pattern>\
     </patterns>\
    </rule>\
  </rules>\
 </ruleset>\
</patterndb>"

/* pdb skeleton used to test patterndb rule actions. E.g. whenever a rule
 * matches, certain actions described in the rule need to be performed.
 * This tests those */
#define pdb_ruletest_skeleton "<patterndb version='5' pub_date='2010-02-22'>\
 <ruleset name='testset' id='1'>\
  <description>This is a test set</description>\
  <patterns>\
    <pattern>prog1</pattern>\
    <pattern>prog2</pattern>\
  </patterns>\
  <rules>\
    <rule provider='test' id='10' class='system' context-scope='program'>\
     <patterns>\
      <pattern>simple-message</pattern>\
     </patterns>\
     <tags>\
      <tag>simple-msg-tag1</tag>\
      <tag>simple-msg-tag2</tag>\
     </tags>\
     <values>\
      <value name='simple-msg-value-1'>value1</value>\
      <value name='simple-msg-value-2'>value2</value>\
      <value name='simple-msg-host'>${HOST}</value>\
     </values>\
    </rule>\
    <rule provider='test' id='10a' class='system' context-scope='program' context-id='$PID' context-timeout='60'>\
     <patterns>\
      <pattern>correllated-message-based-on-pid</pattern>\
     </patterns>\
     <values>\
      <value name='correllated-msg-context-id'>${CONTEXT_ID}</value>\
      <value name='correllated-msg-context-length'>$(context-length)</value>\
     </values>\
    </rule>\
    <rule provider='test' id='10b' class='violation' context-scope='program' context-id='$PID' context-timeout='60'>\
     <patterns>\
      <pattern>correllated-message-with-action-on-match</pattern>\
     </patterns>\
     <actions>\
       <action trigger='match'>\
         <message>\
           <values>\
             <value name='MESSAGE'>generated-message-on-match</value>\
             <value name='context-id'>${CONTEXT_ID}</value>\
           </values>\
           <tags>\
             <tag>correllated-msg-tag</tag>\
           </tags>\
         </message>\
       </action>\
     </actions>\
    </rule>\
    <rule provider='test' id='10c' class='violation' context-scope='program' context-id='$PID' context-timeout='60'>\
     <patterns>\
      <pattern>correllated-message-with-action-on-timeout</pattern>\
     </patterns>\
     <actions>\
       <action trigger='timeout'>\
         <message>\
           <values>\
             <value name='MESSAGE'>generated-message-on-timeout</value>\
           </values>\
         </message>\
       </action>\
     </actions>\
    </rule>\
    <rule provider='test' id='10d' class='violation' context-scope='program' context-id='$PID' context-timeout='60'>\
     <patterns>\
      <pattern>correllated-message-with-action-condition</pattern>\
     </patterns>\
     <actions>\
       <action trigger='match' condition='\"${PID}\" ne \"" MYPID "\"' >\
         <message>\
           <values>\
             <value name='MESSAGE'>not-generated-message</value>\
           </values>\
         </message>\
       </action>\
       <action trigger='match' condition='\"${PID}\" eq \"" MYPID "\"' >\
         <message>\
           <values>\
             <value name='MESSAGE'>generated-message-on-condition</value>\
           </values>\
         </message>\
       </action>\
     </actions>\
    </rule>\
    <rule provider='test' id='10e' class='violation' context-scope='program' context-id='$PID' context-timeout='60'>\
     <patterns>\
      <pattern>correllated-message-with-rate-limited-action</pattern>\
     </patterns>\
     <actions>\
       <action trigger='match' rate='1/60'>\
         <message>\
           <values>\
             <value name='MESSAGE'>generated-message-rate-limit</value>\
           </values>\
         </message>\
       </action>\
     </actions>\
    </rule>\
    <rule provider='test' id='11b' class='violation'>\
     <patterns>\
      <pattern>simple-message-with-action-on-match</pattern>\
     </patterns>\
     <actions>\
       <action trigger='match'>\
         <message>\
           <values>\
             <value name='MESSAGE'>generated-message-on-match</value>\
             <value name='context-id'>${CONTEXT_ID}</value>\
           </values>\
           <tags>\
             <tag>simple-msg-tag</tag>\
           </tags>\
         </message>\
       </action>\
     </actions>\
    </rule>\
    <rule provider='test' id='11d' class='violation'>\
     <patterns>\
      <pattern>simple-message-with-action-condition</pattern>\
     </patterns>\
     <actions>\
       <action trigger='match' condition='\"${PID}\" ne \"" MYPID "\"' >\
         <message>\
           <values>\
             <value name='MESSAGE'>not-generated-message</value>\
           </values>\
         </message>\
       </action>\
       <action trigger='match' condition='\"${PID}\" eq \"" MYPID "\"' >\
         <message>\
           <values>\
             <value name='MESSAGE'>generated-message-on-condition</value>\
           </values>\
         </message>\
       </action>\
     </actions>\
    </rule>\
    <rule provider='test' id='11e' class='violation'>\
     <patterns>\
      <pattern>simple-message-with-rate-limited-action</pattern>\
     </patterns>\
     <actions>\
       <action trigger='match' rate='1/60'>\
         <message>\
           <values>\
             <value name='MESSAGE'>generated-message-rate-limit</value>\
           </values>\
         </message>\
       </action>\
     </actions>\
     <examples>\
       <example>\
         <test_message program='prog1'>simple-message-with-rate-limited-action</test_message>\
         <test_values>\
            <test_value name='PROGRAM'>prog1</test_value>\
            <test_value name='MESSAGE'>foobar</test_value>\
         </test_values>\
       </example>\
       <example>\
         <test_message program='prog2'>simple-message-with-rate-limited-action</test_message>\
       </example>\
     </examples>\
    </rule>\
    <rule provider='test' id='12' class='violation'>\
     <patterns>\
      <pattern>simple-message-with-action-to-create-context</pattern>\
     </patterns>\
     <actions>\
       <action trigger='match'>\
         <create-context context-id='1000' context-timeout='60' context-scope='program'>\
           <message inherit-properties='context'>\
             <values>\
               <value name='MESSAGE'>context message</value>\
             </values>\
           </message>\
         </create-context>\
       </action>\
     </actions>\
    </rule>\
    <rule provider='test' id='13' class='violation' context-id='1000' context-timeout='60' context-scope='program'>\
     <patterns>\
      <pattern>correllated-message-that-uses-context-created-by-rule-id#12</pattern>\
     </patterns>\
     <values>\
       <value name='triggering-message'>${MESSAGE}@1 assd</value>\
     </values>\
    </rule>\
    <rule provider='test' id='14' class='violation' context-id='1001' context-timeout='60' context-scope='program'>\
     <patterns>\
      <pattern>correllated-message-with-action-to-create-context</pattern>\
     </patterns>\
     <values>\
       <value name='rule-msg-context-id'>${.classifier.context_id}</value>\
     </values>\
     <actions>\
       <action trigger='match'>\
         <create-context context-id='1002' context-timeout='60' context-scope='program'>\
           <message inherit-properties='context'>\
             <values>\
               <!-- we should inherit from the LogMessage matching this rule and not the to be created context -->\
               <value name='MESSAGE'>context message ${rule-msg-context-id}</value>\
             </values>\
           </message>\
         </create-context>\
       </action>\
     </actions>\
    </rule>\
    <rule provider='test' id='15' class='violation' context-id='1002' context-timeout='60' context-scope='program'>\
     <patterns>\
      <pattern>correllated-message-that-uses-context-created-by-rule-id#14</pattern>\
     </patterns>\
     <values>\
       <value name='triggering-message'>${MESSAGE}@1 assd</value>\
       <value name='triggering-message-context-id'>$(grep ('${rule-msg-context-id}' ne '') ${rule-msg-context-id})</value>\
     </values>\
    </rule>\
  </rules>\
 </ruleset>\
</patterndb>"

#define pdb_complete_syntax "\
<patterndb version='5' pub_date='2010-02-22'>\
 <ruleset name='testset' id='1'>\
  <url>http://foobar.org/</url>\
  <urls>\
    <url>http://foobar.org/1</url>\
    <url>http://foobar.org/2</url>\
  </urls>\
  <description>This is a test set</description>\
  <patterns>\
    <pattern>prog2</pattern>\
    <pattern>prog3</pattern>\
  </patterns>\
  <pattern>prog1</pattern>\
  <rules>\
    <rule provider='test' id='10' class='system' context-id='foobar' context-scope='program'>\
     <description>This is a rule description</description>\
     <urls>\
       <url>http://foobar.org/1</url>\
       <url>http://foobar.org/2</url>\
     </urls>\
     <patterns>\
      <pattern>simple-message</pattern>\
      <pattern>simple-message-alternative</pattern>\
     </patterns>\
     <tags>\
      <tag>simple-msg-tag1</tag>\
      <tag>simple-msg-tag2</tag>\
     </tags>\
     <values>\
      <value name='simple-msg-value-1'>value1</value>\
      <value name='simple-msg-value-2'>value2</value>\
      <value name='simple-msg-host'>${HOST}</value>\
     </values>\
     <examples>\
       <example>\
         <test_message program='foobar'>This is foobar message</test_message>\
         <test_values>\
           <test_value name='foo'>foo</test_value>\
           <test_value name='bar'>bar</test_value>\
         </test_values>\
       </example>\
     </examples>\
     <actions>\
       <action>\
         <message>\
           <values>\
             <value name='FOO'>foo</value>\
             <value name='BAR'>bar</value>\
           </values>\
           <tags>\
             <tag>tag1</tag>\
             <tag>tag2</tag>\
           </tags>\
         </message>\
       </action>\
       <action>\
         <create-context context-id='foobar'>\
           <message>\
             <values>\
               <value name='FOO'>foo</value>\
               <value name='BAR'>bar</value>\
             </values>\
             <tags>\
               <tag>tag1</tag>\
               <tag>tag2</tag>\
             </tags>\
             </message>\
         </create-context>\
       </action>\
     </actions>\
    </rule>\
  </rules>\
</ruleset>\
</patterndb>"

#define pdb_inheritance_enabled_skeleton "<patterndb version='4' pub_date='2010-02-22'>\
  <ruleset name='testset' id='1'>\
    <patterns>\
      <pattern>prog2</pattern>\
    </patterns>\
    <rules>\
      <rule provider='test' id='11' class='system'>\
        <patterns>\
          <pattern>pattern-with-inheritance-enabled</pattern>\
        </patterns>\
        <tags>\
          <tag>basetag1</tag>\
          <tag>basetag2</tag>\
        </tags>\
        <actions>\
          <action trigger='match'>\
            <message inherit-properties='TRUE'>\
              <values>\
                <value name='actionkey'>actionvalue</value>\
              </values>\
              <tags>\
                <tag>actiontag</tag>\
              </tags>\
            </message>\
          </action>\
        </actions>\
      </rule>\
    </rules>\
  </ruleset>\
</patterndb>"

#define pdb_inheritance_disabled_skeleton "<patterndb version='4' pub_date='2010-02-22'>\
  <ruleset name='testset' id='1'>\
    <patterns>\
      <pattern>prog2</pattern>\
    </patterns>\
    <rules>\
      <rule provider='test' id='12' class='system'>\
        <patterns>\
          <pattern>pattern-with-inheritance-disabled</pattern>\
        </patterns>\
        <tags>\
          <tag>basetag1</tag>\
          <tag>basetag2</tag>\
        </tags>\
        <actions>\
          <action trigger='match'>\
            <message inherit-properties='FALSE'>\
              <values>\
                <value name='actionkey'>actionvalue</value>\
              </values>\
              <tags>\
                <tag>actiontag</tag>\
              </tags>\
            </message>\
          </action>\
        </actions>\
      </rule>\
    </rules>\
 </ruleset>\
</patterndb>"

#define pdb_inheritance_context_skeleton "\
<patterndb version='4' pub_date='2010-02-22'>\
  <ruleset name='testset' id='1'>\
    <patterns>\
      <pattern>prog2</pattern>\
    </patterns>\
    <rules>\
      <rule provider='test' id='11' class='system' context-scope='program'\
           context-id='$PID' context-timeout='60'>\
        <patterns>\
          <pattern>pattern-with-inheritance-context</pattern>\
        </patterns>\
        <tags>\
          <tag>basetag1</tag>\
          <tag>basetag2</tag>\
        </tags>\
        <actions>\
          <action trigger='timeout'>\
            <message inherit-properties='context'>\
              <values>\
                <value name='MESSAGE'>action message</value>\
              </values>\
              <tags>\
                <tag>actiontag</tag>\
              </tags>\
            </message>\
          </action>\
        </actions>\
     </rule>\
    </rules>\
  </ruleset>\
</patterndb>"

#define pdb_msg_count_skeleton "<patterndb version='4' pub_date='2010-02-22'>\
 <ruleset name='testset' id='1'>\
  <patterns>\
   <pattern>prog1</pattern>\
   <pattern>prog2</pattern>\
  </patterns>\
  <rules>\
    <rule provider='test' id='13' class='system' context-scope='program'\
          context-id='$PID' context-timeout='60'>\
      <patterns>\
        <pattern>pattern13</pattern>\
      </patterns>\
      <values>\
        <value name='n13-1'>v13-1</value>\
      </values>\
      <actions>\
        <action condition='\"${n13-1}\" eq \"v13-1\"' trigger='match'>\
          <message inherit-properties='TRUE'>\
            <values>\
              <value name='CONTEXT_LENGTH'>$(context-length)</value>\
            </values>\
          </message>\
        </action>\
      </actions>\
    </rule>\
    <rule provider='test' id='14' class='system' context-scope='program'\
          context-id='$PID' context-timeout='60'>\
      <patterns>\
        <pattern>pattern14</pattern>\
      </patterns>\
      <actions>\
        <action condition='\"$(context-length)\" eq \"1\"' trigger='match'>\
          <message inherit-properties='TRUE'>\
            <values>\
              <value name='CONTEXT_LENGTH'>$(context-length)</value>\
            </values>\
          </message>\
        </action>\
      </actions>\
    </rule>\
    <rule provider='test' id='15' class='system' context-scope='program'\
          context-id='$PID' context-timeout='60'>\
      <patterns>\
        <pattern>pattern15@ANYSTRING:p15@</pattern>\
      </patterns>\
      <actions>\
        <action condition='\"$(context-length)\" eq \"2\"' trigger='match'>\
          <message inherit-properties='FALSE'>\
            <values>\
              <value name='fired'>true</value>\
            </values>\
          </message>\
        </action>\
      </actions>\
    </rule>\
  </rules>\
 </ruleset>\
</patterndb>"

#define pdb_tag_outside_of_rule_skeleton "<patterndb version='3' pub_date='2010-02-22'>\
 <ruleset name='testset' id='1'>\
  <patterns>\
   <pattern>prog1</pattern>\
  </patterns>\
  <tags>\
   <tag>tag1</tag>\
  </tags>\
 </ruleset>\
</patterndb>"

#endif
