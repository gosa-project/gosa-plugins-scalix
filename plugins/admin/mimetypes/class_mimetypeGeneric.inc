<?php
class mimetype extends plugin
{
	/* CLI vars */
	var $cli_summary		  = "This tab allows you to modify, add or remove mime types in the selected department.";
	var $cli_description	= "Management dialog to handle mime types";
	var $ignore_account 	= true;	// This is always a valid account
	var $cli_parameters		= array("eins" => "Eins ist toll", "zwei" => "Zwei ist noch besser");
	var $objectclasses		= array("top","gotoMimeType");
	var $attributes			  = array("cn","gotoMimeApplication","gotoMimeLeftClickAction","gotoMimeIcon",
									"description","gotoMimeFilePattern","gotoMimeEmbeddedApplication",
									"gotoMimeGroup");	

	/* Class vars */
	var $acl;
	var $ui;
	var $cn									              = "";					
	var $gotoMimeLeftClickAction			    = "I";
	var $gotoMimeLeftClickAction_I		    = true;
	var $gotoMimeLeftClickAction_E		    = false;
	var $gotoMimeLeftClickAction_Q		    = false;
	var $gotoMimeIcon						          = "*removed*";
	var $description						          = "";
	var $gotoMimeFilePattern				      = array();
	var $gotoMimeApplication				      = array();
	var $gotoMimeEmbeddedApplication	    = array();
	var $gotoMimeGroup						        = "";
	var $iconData							            = NULL;	
	var $base								              = "";

  /* To prevent errors when using the 'apply' button, we use this variables 
      to manage array attributes */
	var $use_gotoMimeIcon					        = NULL;
	var $use_gotoMimeFilePattern			    = array();
	var $use_gotoMimeApplication			    = array();
	var $use_gotoMimeEmbeddedApplication	= array();

  /* divLists */
  var $DivPatterns						          = NULL;
  var $DivApps							            = NULL;
  var $DivEApps							            = NULL;

  /* Mime type release mode */
  var $isReleaseMimeType 					      = false;
  var $dialog								            = NULL;

  /* These vars will be copied too, if you use copy&paste mode */
  var $CopyPasteVars        = array("use_gotoMimeFilePattern","use_gotoMimeApplication","use_gotoMimeEmbeddedApplication","iconData");

  /* Select options */
  var $MimeGroups			      = array("application","audio","chemical","image","inode","message","model",
                                      "multipart","text","video","x-conference","x-world");

  /* Orig_Dn is used to verify if this object is new or only edited */		
  var $orig_dn			= "";


  function mimetype($config,$dn= NULL)
  {
    plugin::plugin ($config, $dn);

    /* Save original dn */
    $this->orig_dn = $dn;	

    /* get gotoMimeLeftClickActions I/E/Q */
    if(isset($this->gotoMimeLeftClickAction)){
      $str = $this->gotoMimeLeftClickAction;
      for($i = 0 ; $i < strlen($str) ; $i ++ ){
        $varna = "gotoMimeLeftClickAction_". $str[$i];
        if(isset($this->$varna)){
          $this->$varna = true;
        }
      }
    }

    /* If both radio buttons arn't set, set option I */
    if(!$this->gotoMimeLeftClickAction_I && !$this->gotoMimeLeftClickAction_E){
      $this->gotoMimeLeftClickAction_I = true;
    }

    /* Get list of array attributes */
    foreach(array("gotoMimeFilePattern") as $attr){
      $this->$attr = array();
      if(isset($this->attrs[$attr])){
        $tmp = array();
        for($i = 0 ; $i < $this->attrs[$attr]['count'] ; $i ++){
          $str = $this->attrs[$attr][$i];
          $tmp[] = $str;
        }
        $use_attr = "use_".$attr;
        $this->$use_attr = $tmp;
      }
    }

    /* Get list of array attributes with priority tag ( Test|32 )*/
    foreach(array("gotoMimeApplication","gotoMimeEmbeddedApplication") as $attr){
      $this->$attr = array();
      if(isset($this->attrs[$attr])){
        $tmp = array();
        for($i = 0 ; $i < $this->attrs[$attr]['count'] ; $i ++){
          $str = $this->attrs[$attr][$i];
          $tmp2= split("\|",$str);

          if(count($tmp2) == 2){
            $name = $tmp2[0];
            $prio = $tmp2[1];
            $tmp[$prio] = $name;
          }
        }
        ksort($tmp);
        $use_attr = "use_".$attr;
        $this->$use_attr = $tmp;
      }
    }

    /* Check if release Management is enabled */
    $tmp = search_config($this->config->data,"faiManagement","CLASS");
    if(!empty($tmp)) {
      $this->isReleaseMimeType= true;
    }

    /* Set base */	
    if ($this->dn == "new"){
      if(isset($_SESSION['CurrentMainBase'])){
        $this->base= $_SESSION['CurrentMainBase'];
      }else{
        $ui= get_userinfo();
        $this->base= dn2base($ui->dn);
      }
    } else {
      $this->base= preg_replace ("/^[^,]+,[^,]+,/", "", $this->dn);
    }

    /* Get icon data */
    if(isset($this->attrs['gotoMimeIcon'])){
      $ldap = $this->config->get_ldap_link();
      $this->iconData = $ldap->get_attribute($this->dn,"gotoMimeIcon");
      $this->saved_attributes['gotoMimeIcon'] = $this->iconData;
    }
    if ($this->iconData == ""){
      $this->set_new_picture("");
    }
    $_SESSION['binary']			= $this->iconData;
    $_SESSION['binarytype']		= "image/jpeg";
  }


  function execute()
  {
    $smarty = get_smarty();

    /* Base select dialog */
    $once = true;
    foreach($_POST as $name => $value){
      if(preg_match("/^chooseBase/",$name) && $once){
        $once = false;
        $this->dialog = new baseSelectDialog($this->config,$this);
        echo $this->base;
        $this->dialog->setCurrentBase($this->base);
      }
    }

    /* Dialog handling */
    if(is_object($this->dialog)){

      /* Must be called before save_object */
      $this->dialog->save_object();

      if($this->dialog->isClosed()){
        $this->dialog = false;
      }elseif($this->dialog->isSelected()){
        $this->base = $this->dialog->isSelected();
        $this->dialog= false;
      }else{
        return($this->dialog->execute());
      }
    }

    /* Check Posts */
    $posts = array(	
        "/^Pattern_SortUp_/" 	=> array("Action" => "Pattern_SortUp",	"Func" => "ArrayUp",	"Attr" => "use_gotoMimeFilePattern"),
        "/^Pattern_SortDown_/" 	=> array("Action" => "Pattern_SortDown","Func" => "ArrayDown",	"Attr" => "use_gotoMimeFilePattern"),
        "/^Pattern_Remove_/" 	=> array("Action" => "Pattern_Remove",	"Func" => "ArrayRemove","Attr" => "use_gotoMimeFilePattern"),
        "/^Apps_SortUp_/" 		=> array("Action" => "Apps_SortUp",		"Func" => "ArrayUp",    "Attr" => "use_gotoMimeApplication"),
        "/^Apps_SortDown_/" 	=> array("Action" => "Apps_SortDown",	"Func" => "ArrayDown",  "Attr" => "use_gotoMimeApplication"),
        "/^Apps_Remove_/" 		=> array("Action" => "Apps_Remove",		"Func" => "ArrayRemove","Attr" => "use_gotoMimeApplication"),
        "/^EApps_SortUp_/" 		=> array("Action" => "EApps_SortUp",	"Func" => "ArrayUp",    "Attr" => "use_gotoMimeEmbeddedApplication"),
        "/^EApps_SortDown_/" 	=> array("Action" => "EApps_SortDown",	"Func" => "ArrayDown",  "Attr" => "use_gotoMimeEmbeddedApplication"),
        "/^EApps_Remove_/" 		=> array("Action" => "EApps_Remove",	"Func" => "ArrayRemove","Attr" => "use_gotoMimeEmbeddedApplication"));		
    $once = true;

    /* Walk through posts and try to find some commands for us. */
    foreach($_POST as $name => $value){

      /* Walk through possible commands */
      foreach($posts as $regex => $action){

        /* Check if there is a command posted */
        if(preg_match($regex,$name) && $once){
          $once = false;

          /* Get action vars */
          $func 		= $action['Func'];		// Get function name 
          $attr 		= $action['Attr'];		// Get attribute name

          /* Get entry id */
          $s_entry  	= preg_replace($regex,"",$name);
          $s_entry 	= preg_replace("/_[xy]$/","",$s_entry); 

          /* Execute a command with the given attribute and entry 
             e.g. $this->gotoMimeFilePattern = $this->ArrayUp(3,$this->gotoMimeFilePattern) */
          if(chkacl($this->acl,$attr) == ""){
            $this->$attr= $this->$func($s_entry,$this->$attr,true);					 
          }
        }
      }
    }

    /* Set a new icon was requested */
    if(isset($_POST['update_icon']) && (isset($_FILES['picture_file']['name']))){
      $this->set_new_picture($_FILES['picture_file']['tmp_name']);		
    }

    /* Add gotoMimeFilePattern */
    if(isset($_POST['AddNewFilePattern']) && isset($_POST['NewFilePattern'])){
      if(chkacl($this->acl,"gotoMimeFilePattern") == ""){
        $str = $_POST['NewFilePattern'];
        if(!empty($str)){
          $this->use_gotoMimeFilePattern[] = $str;
        }
      }
    }	

    /* Add gotoMimeFilePattern */
    if(isset($_POST['AddNewApplication']) && isset($_POST['NewApplication'])){
      if(chkacl($this->acl,"gotoMimeApplication") == ""){
        $str = $_POST['NewApplication'];
        if(!empty($str)){
          $this->use_gotoMimeApplication[] = $str;
        }
      }
    }	

    /* Add gotoMimeFilePattern */
    if(isset($_POST['AddNewEmbeddedApplication']) && isset($_POST['NewEmbeddedApplication'])){
      if(chkacl($this->acl,"gotoMimeEmbeddedApplication") == ""){
        $str = $_POST['NewEmbeddedApplication'];
        if(!empty($str)){
          $this->use_gotoMimeEmbeddedApplication[] = $str;
        }
      }
    }	

    /* Create divlists */
    $DivPatterns 	= new DivSelectBox("gotoMimePatterns");
    $DivApps 		= new DivSelectBox("gotoMimeApplications");
    $DivEApps 		= new DivSelectBox("gotoMimeEmbeddedApplications");
    $DivPatterns	-> SetHeight(100);
    $DivApps		-> SetHeight(100);
    $DivEApps		-> SetHeight(100);


    if(chkacl($this->acl,"gotoMimeFilePattern") == ""){
      $Pattern_Actions= " <input type='image' src='images/sort_up.png' 	class='center'	name='Pattern_SortUp_%s' >&nbsp;
      <input type='image' src='images/sort_down.png' 	class='center'  name='Pattern_SortDown_%s'>&nbsp;
      <input type='image' src='images/edittrash.png'	class='center'  name='Pattern_Remove_%s'>";
    }else{
      $Pattern_Actions= "";	
    }

    if(chkacl($this->acl,"gotoMimeApplication") == ""){
      $Apps_Actions 	= " <input type='image' src='images/sort_up.png' 	class='center'	name='Apps_SortUp_%s' >&nbsp;
      <input type='image' src='images/sort_down.png' 	class='center'  name='Apps_SortDown_%s'>&nbsp;
      <input type='image' src='images/edittrash.png'	class='center'  name='Apps_Remove_%s'>";
    }else{
      $Apps_Actions= "";	
    }

    if(chkacl($this->acl,"gotoMimeEmbeddedApplication") == ""){
      $EApps_Actions 	= " <input type='image' src='images/sort_up.png' 	class='center'	name='EApps_SortUp_%s' >&nbsp;
      <input type='image' src='images/sort_down.png' 	class='center'  name='EApps_SortDown_%s'>&nbsp;
      <input type='image' src='images/edittrash.png'	class='center'  name='EApps_Remove_%s'>";
    }else{
      $EApps_Actions= "";	
    }

    foreach($this->use_gotoMimeFilePattern as $key => $pattern){
      $field1 = array("string" => $pattern);
      $field2 = array("string" => preg_replace("/%s/",$key,$Pattern_Actions),"attach"=>"style='border-right:0px;width:50px;'");
      $fields = array($field1,$field2);
      $DivPatterns -> AddEntry($fields);
    }			
    foreach($this->use_gotoMimeApplication as $key => $pattern){
      $field1 = array("string" => $pattern);
      $field2 = array("string" => preg_replace("/%s/",$key,$Apps_Actions),"attach"=>"style='border-right:0px;width:50px;'");
      $fields = array($field1,$field2);
      $DivApps -> AddEntry($fields);
    }			
    foreach($this->use_gotoMimeEmbeddedApplication as $key => $pattern){
      $field1 = array("string" => $pattern);
      $field2 = array("string" => preg_replace("/%s/",$key,$EApps_Actions),"attach"=>"style='border-right:0px;width:50px;'");
      $fields = array($field1,$field2);
      $DivEApps -> AddEntry($fields);
    }
    $smarty->assign("baseACL", 						chkacl($this->acl,"base"));	
    $smarty->assign("bases", 						$this->config->idepartments);		
    $smarty->assign("base_select", 					$this->base);		
    $smarty->assign("isReleaseMimeType",			$this->isReleaseMimeType);
    $smarty->assign("gotoMimeFilePatterns",			$DivPatterns->DrawList());
    $smarty->assign("gotoMimeApplications",			$DivApps->DrawList());
    $smarty->assign("gotoMimeEmbeddedApplications",	$DivEApps->DrawList());

    /* Assign class vars to smarty */
    foreach($this->attributes as $attr){
      $smarty->assign($attr,$this->$attr);
      $smarty->assign($attr."ACL",chkacl($this->acl,$attr));
    }	

    /* Assign additional vars that are not included in attributes*/
    foreach(array("gotoMimeLeftClickAction_I","gotoMimeLeftClickAction_E","gotoMimeLeftClickAction_Q") as $attr){
      $smarty->assign($attr,$this->$attr);
    }	

    /* Assign select box options */
    $smarty->assign("gotoMimeGroups",$this->MimeGroups);
    $smarty->assign("gotoMimeIcon"	,$this->get_picture());
    return($smarty->fetch(get_template_path("generic.tpl",TRUE,dirname(__FILE__))));
  }


  function save_object()
  {
    if(isset($_POST['MimeGeneric'])){
      plugin::save_object();

      /* Only save base if we are not in release mode */
      if(!$this->isReleaseMimeType){
        if(isset($_POST['base'])){
          $this->base = $_POST['base'];
        }
      }

      /* Save radio buttons */
      if(chkacl($this->acl,"gotoMimeLeftClickAction")== ""){
        if(isset($_POST['gotoMimeLeftClickAction_IE'])){
          $chr = $_POST['gotoMimeLeftClickAction_IE'];
          if($chr == "E"){
            $this->gotoMimeLeftClickAction_E = true;
            $this->gotoMimeLeftClickAction_I = false;
          }else{
            $this->gotoMimeLeftClickAction_E = false;
            $this->gotoMimeLeftClickAction_I = true;
          }
        }
        if(isset($_POST['gotoMimeLeftClickAction_Q'])){
          $this->gotoMimeLeftClickAction_Q = true;
        }else{
          $this->gotoMimeLeftClickAction_Q = false;
        }
      }
    }
  }


  /* save current changes */
  function save()
  {
    /* Create gotoMimeLeftClickAction out of checkboxes and radio buttons */	
    $arr = array ("E","I","Q");
    $str = "";
    foreach ($arr as $Chr){
      $var = "gotoMimeLeftClickAction_".$Chr;
      if($this->$var){
        $str .= $Chr;
      }
    }
    $this->gotoMimeLeftClickAction = $str;

    /* Create array entries with priority tag ( Test|3 )*/
    foreach(array("gotoMimeEmbeddedApplication","gotoMimeApplication") as $attr){
      $i 				= 0;
      $use_attr 		= "use_".$attr;
      $tmp 			= array();
      $this->$attr 	= array();
      foreach($this->$use_attr as $entry){
        $tmp[] = $entry."|".$i ++;
      }
      $this->$attr = $tmp;
    }

    /* Create array entries */
    foreach(array("gotoMimeFilePattern") as $attr){
      $i 				= 0;
      $use_attr 		= "use_".$attr;
      $tmp 			= array();
      $this->$attr 	= array();
      foreach($this->$use_attr as $entry){
        $tmp[] = $entry;
      }
      $this->$attr = $tmp;
    }

    /* Remove Icon if requested  */
    if($this->use_gotoMimeIcon != "*removed*"){
      $this->gotoMimeIcon = $this->iconData;
    }else{
      $this->gotoMimeIcon = "";
    }

    plugin::save();

    /* If this is a newly created object, skip storing those 
       attributes that contain an empty array */
    if($this->orig_dn == "new"){
      foreach(array("gotoMimeEmbeddedApplication","gotoMimeApplication","gotoMimeFilePattern") as $attr){
        if(!count($this->$attr)){
          unset($this->attrs[$attr]);
        }
      }
    }

    $ldap = $this->config->get_ldap_link();
    $ldap-> cd ( $this->config->current['BASE']);
    $ldap->cat($this->dn);
    if($ldap->count()){
      $ldap->cd($this->dn);
      $this->cleanup();
      $ldap->modify($this->attrs);
    }else{
      $ldap->create_missing_trees(preg_replace('/^[^,]+,/', '', $this->dn));
      $ldap->cd($this->dn);
      $ldap->add($this->attrs);
    }
    show_ldap_error($ldap->get_error(), sprintf(_("Saving of mime type/generic with dn '%s' failed."),$this->dn));
  }


  /* Remove current mime type */
  function remove_from_parent()
  {
    $ldap = $this->config->get_ldap_link();
    $ldap->rmDir($this->dn);
    show_ldap_error($ldap->get_error(), sprintf(_("Removing of mime type/generic with dn '%s' failed."),$this->dn));

    /* Optionally execute a command after we're done */
    $this->handle_post_events("remove");

    /* Delete references to object groups */
    $ldap->cd ($this->config->current['BASE']);
    $ldap->search ("(&(objectClass=gosaGroupOfNames)(member=".$this->dn."))", array("cn"));
    while ($ldap->fetch()){
      $og= new ogroup($this->config, $ldap->getDN());
      unset($og->member[$this->dn]);
      $og->save ();
      show_ldap_error($ldap->get_error(), sprintf(_("Removing mime type from objectgroup '%s' failed"), $og->dn));
    }
  }


  /* Check given values */
  function check()
  {
    $message = plugin::check();
    if(empty($this->cn)){
      $message[] = _("Please specify a valid name for this mime type.");
    }
    if(!count($this->use_gotoMimeFilePattern)){
      $message[] = _("Please specify at least one file pattern.") ;
    }

    /* Check if there is already a mime type with this cn */
    $ldap = $this->config->get_ldap_link();
    $ldap->cd($this->config->current["BASE"]);
    if($this->isReleaseMimeType && (isset($_SESSION['mimefilter']['release']))){
      $baseDn = str_replace($this->config->current['BASE'],$this->base,$_SESSION['mimefilter']['release']);
      $baseDn = preg_replace("/ou=mime,.*/","ou=mime,".$this->base,$_SESSION['mimefilter']['release']);
      $ldap->ls("(&(objectClass=gotoMimeType)(cn=".$this->cn."))",$baseDn,array("cn"));
      if($ldap->count()){
        $attrs = $ldap->fetch();
        if($this->dn != $attrs['dn']) {
          $message[]= _("There's already a mime type with this 'Name'.");
        }
      }
    }else{
      $ldap->ls("(&(objectClass=gotoMimeType)(cn=".$this->cn."))","ou=mime,".$this->base,array("cn"));
      if ($ldap->count()){
        $attrs = $ldap->fetch();
        if($this->dn != $attrs['dn']) {
          $message[]= _("There's already an mime with this 'Name'.");
        }
      }
    }

    /* Check permissions for that base? */
    if ($this->base != ""){
      $new_dn= "cn=".$this->cn.",ou=mime,".$this->base;
    } else {
      $new_dn= $this->dn;
    }
    if (chkacl($this->acl, "create") != ""){
      $message[]= _("You have no permissions to create a mime type on this 'Base'.");
    }

    return($message);
  }

  /** Helper functions **/

  /* Set a new picture */	
  function set_new_picture($filename)
  {
    if (empty($filename)){
      $filename= "./images/default_icon.png";
      $this->use_gotoMimeIcon= "*removed*";
    }

    if (file_exists($filename)){
      $fd = fopen ($filename, "rb");
      $this->iconData= fread ($fd, filesize ($filename));
      $_SESSION['binary']= $this->iconData;
      $_SESSION['binarytype']= "image/jpeg";
      fclose ($fd);
    }
  }

  /* Get picture link */
  function get_picture()
  {
    $_SESSION['binary']= $this->iconData;
    $_SESSION['binarytype']= "image/jpeg";
    return("getbin.php");
  }

  /* Transports the given Arraykey one position up*/
  function ArrayUp($atr,$attrs)
  {
    $ret = $attrs;
    $pos = $this->getpos($atr,$attrs) ;
    $cn = count($attrs);
    if(!(($pos == -1)||($pos == 1))){
      $before = array_slice($attrs,0,($pos-2));
      $mitte  = array_reverse(array_slice($attrs,($pos-2),2));
      $unten  = array_slice($attrs,$pos);
      $ret = array();
      $ret = $this->combineArrays($before,$mitte,$unten);
    }
    return($ret);
  }


  /* Transports the given Arraykey one position down*/
  function ArrayDown($atr,$attrs)
  {
    $ret = $attrs;
    $pos = $this->getpos($atr,$attrs) ;
    $cn = count($attrs);
    if(!(($pos == -1)||($pos == $cn))){
      $before = array_slice($attrs,0,($pos-1));
      $mitte  = array_reverse(array_slice($attrs,($pos-1),2));
      $unten  = array_slice($attrs,($pos+1));
      $ret = array();
      $ret = $this->combineArrays($before,$mitte,$unten);
    }
    return($ret);
  }


  /* return the position of the element in the array */
  function getpos($atr,$attrs)
  {
    $i = 0;
    foreach($attrs as $attr => $name)    {
      $i++;
      if($attr == $atr){
        return($i);
      }
    }
    return(-1);
  }


  /* Remove this element from array */
  function ArrayRemove($key,$array,$reorder = false)	
  {
    if(isset($array[$key])){
      unset($array[$key]);
      if($reorder){
        $tmp = array();
        foreach($array as $entry){
          $tmp[] = $entry;
        }
        $array = $tmp;
      }
    }	
    return($array);
  }


  /* Combine new array */
  function combineArrays($ar0,$ar1,$ar2)
  {
    $ret = array();
    if(is_array($ar0))
      foreach($ar0 as $ar => $a){
        $ret[]=$a;
      }
    if(is_array($ar1))
      foreach($ar1 as $ar => $a){
        $ret[]=$a;
      }
    if(is_array($ar2))
      foreach($ar2 as $ar => $a){
        $ret[]=$a;
      }
    return($ret);
  }


  /* Return a dialog with all fields that must be changed, 
     if we want to copy this entry */
  function getCopyDialog()
  {
    $str = "";

    $smarty = get_smarty();
    $smarty->assign("cn",     		$this->cn);
    $smarty->assign("description",  $this->description);
    $str = $smarty->fetch(get_template_path("paste_generic.tpl",TRUE,dirname(__FILE__)));

    $ret = array();
    $ret['string'] = $str;
    $ret['status'] = "";
    return($ret);
  }


  /* Save all */
  function saveCopyDialog()
  {
    $attrs = array("cn","description");
    foreach($attrs as $attr){
      if(isset($_POST[$attr])){
        $this->$attr = $_POST[$attr];
      }
    }
  }

  /* Return plugin informations for acl handling  */ 
  function plInfo()
  {
    return (array(
          "plShortName"   => _("Generic"),
          "plDescription" => _("Mime type generic"),
          "plSelfModify"  => FALSE,
          "plDepends"     => array(),
          "plPriority"    => 0,
          "plSection"     => array("administration"),
          "plCategory"    => array("mimetypes" => array("description"  => _("Mime types"),
                                                        "objectClass"  => "gotoMimeType")),
          "plProvidedAcls"=> array(
            "cn"                          => _("Name"),
            "gotoMimeGroup"               => _("Mime group"),
            "description"                 => _("Description"),
            "base"                        => _("Base"),
            "gotoMimeApplication"         => _("Application"),
            "gotoMimeLeftClickAction"     => _("Left click action"),
            "gotoMimeIcon"                => _("Icon"),
            "gotoMimeFilePattern"         => _("File patterns"),
            "gotoMimeEmbeddedApplication" => _("Embedded applications"))
          ));

  }
}
// vim:tabstop=2:expandtab:shiftwidth=2:filetype=php:syntax:ruler:
?>