
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                version="1.0">

<xsl:import href="/usr/share/sgml/docbook/stylesheet/xsl/nwalsh/html/docbook.xsl"/>
<xsl:param name="chunk.first.sections" select="1"/>
<xsl:param name="section.autolabel" select="1"/>
<xsl:param name="navig.graphics" select="1"/>
<xsl:param name="navig.graphics.extension" select="'.png'"/>
<xsl:param name="suppress.footer.navigation" select="1"/>
<xsl:param name="html.stylesheet" select="'static/refguide.css'"/>
<xsl:param name="table.borders.with.css" select="0"/>

<xsl:template name="header.navigation">
  <xsl:param name="prev" select="/foo"/>
  <xsl:param name="next" select="/foo"/>
  <xsl:param name="nav.context"/>
                                                                                                           
  <xsl:variable name="home" select="/*[1]"/>
  <xsl:variable name="up" select="parent::*"/>

  <div class="navheader">
     <table width="100%" summary="Navigation header">
       <tr>
         <td>
           <b><xsl:apply-templates select="/" mode="object.title.markup"/></b>
         </td>
         <td width="100">
           <img src="balabit.png"/>
         </td>
       </tr>
       <tr>
         <td colspan="2">
           <xsl:choose>
             <xsl:when test="count($up)>0">
               <a class="naviglink">
                 <xsl:attribute name="href">
                   <xsl:call-template name="href.target">
                     <xsl:with-param name="object" select="$up"/>
                   </xsl:call-template>
                 </xsl:attribute>
                 Up
               </a>
             </xsl:when>
             <xsl:otherwise>
               Up
             </xsl:otherwise>
           </xsl:choose>
           <xsl:choose>
             <xsl:when test="count($prev)>0">
               <a class="naviglink">
                 <xsl:attribute name="href">
                   <xsl:call-template name="href.target">
                     <xsl:with-param name="object" select="$prev"/>
                   </xsl:call-template>
                 </xsl:attribute>
                 Previous
               </a>
             </xsl:when>
             <xsl:otherwise>
               Previous
             </xsl:otherwise>
           </xsl:choose>           <xsl:choose>
             <xsl:when test="count($next)>0">
               <a class="naviglink">
                 <xsl:attribute name="href">
                   <xsl:call-template name="href.target">
                     <xsl:with-param name="object" select="$next"/>
                   </xsl:call-template>
                 </xsl:attribute>
                 Next
               </a>
             </xsl:when>
             <xsl:otherwise>
               Next
             </xsl:otherwise>
           </xsl:choose>
         </td>
       </tr>
     </table>
     <hr height="1"/>
   </div>
</xsl:template>

</xsl:stylesheet>
                
                                                