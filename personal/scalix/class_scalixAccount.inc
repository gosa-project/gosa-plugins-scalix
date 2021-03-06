<?php
/*! \brief   scalix plugin
  \author  Jörn Dreyer <gosa.jfd@butonic.de>,
           based on work by Cajus Pollmeier <pollmeier@gonicus.de>
  \version 0.01
  \date    20.10.2006

  This class provides the functionality to read and write all attributes
  relevant for scalixAccounts from/to the LDAP. It does syntax checking
  and displays the formulars required.
 */

class scalixAccount extends plugin
{
  /* Definitions */
  var $plHeadline   = "SCALIX";
  var $plDescription= "Manage personal SCALIX settings";

  /* plugin specific values - scalixUserClass */
  /* MUST */
  var $scalixScalixObject= TRUE; //TRUE or FALSE
  var $scalixMailnode= "";

  /* Indicates whether we have a scalix group or a sclix user account 
   */
	var $scalixGroup = FALSE;
  
  /* INDIRECT MUST, scalix will complain if no email was specified */
  /* MAY */
  var $scalixAdministrator= FALSE;
  var $scalixMailboxAdministrator = FALSE;
  var $scalixServerLanguage= "";
  var $scalixEmailAddress= array();
  var $scalixLimitMailboxSize= "";
  var $scalixLimitOutboundMail= FALSE;
  var $scalixLimitInboundMail= FALSE;
  var $scalixLimitNotifyUser= FALSE;
  var $scalixHideUserEntry= FALSE;
  var $scalixMailboxClass= "";

  var $mail ="";
  var $uid  ="";
  var $default_permissions= "none";
  var $member_permissions= "post";
  var $members= array();
  var $admins= array();
  var $vacations= array();
  var $perms= array(  "lrs"       => "read", 
                      "lrsp"      => "post", 
                      "lrsip"     => "append",
                      "lrswipcd"  => "write", 
                      "lrswipcda" => "all" );

  /* attribute list for save action */
  var $attributes           = array("scalixMailnode", "scalixServerLanguage", "scalixLimitMailboxSize", "scalixMailboxClass");
  var $fakeBooleanAttributes= array("scalixAdministrator", "scalixMailboxAdministrator", "scalixLimitOutboundMail",
                                    "scalixLimitInboundMail", "scalixLimitNotifyUser", "scalixHideUserEntry");
  var $objectclasses        = array("scalixUserClass");


  /* constructor, if 'dn' is set, the node loads the given
     'dn' from LDAP */
  function __construct(&$config, $dn= NULL)
  {
    /* dn shouldn't be NULL */
    if($dn === NULL){
      trigger_error("Initialising scalixAccount without valid dn.");
    }

    /* Load bases attributes */
    plugin::__construct($config, $dn);

    /* Load attributes */
    if ($dn != "new"){

      /* Set user id */
      if(isset($this->attrs['uid'][0])){
        $this->uid = $this->attrs['uid'][0];
      }
	
      foreach ($this->fakeBooleanAttributes as $val){
        if (isset($this->attrs["$val"][0])&&$this->attrs["$val"][0]=="TRUE"){
          $this->$val = TRUE;
        } else {
          $this->$val = FALSE;
        }
      }

      /* Load attributes containing arrays */
      foreach (array("scalixEmailAddress") as $val){
        if (isset($this->attrs["$val"]["count"])){
          for ($i= 0; $i<$this->attrs["$val"]["count"]; $i++){
            array_push($this->$val, $this->attrs["$val"][$i]);
          }
        }
      }
    }

    /* Save initial account state */
    $this->initially_was_account= $this->is_account;
  }


  function execute()
  {
    /* Call parent execute */
    plugin::execute();

    /* Load templating engine */
    $smarty= get_smarty();
    $smarty->assign("scalixGroup", set_post($this->scalixGroup));
    $display= "";

    /* Do we need to flip is_account state? */
    if (isset($_POST['modify_state'])){
      $this->is_account= !$this->is_account;
    }

    /* Show main page */
    $mailboxClasses = array("&nbsp;", "LIMITED", "FULL");
    $serverLanguages= array("&nbsp;", "GERMAN", "ENGLISH");

    /* Do we represent a valid account? */
    if (!$this->is_account && $this->parent === NULL){
      $display= "<img alt=\"\" src=\"images/small-error.png\" align=\"middle\">&nbsp;<b>".
        _("This account has no SCALIX extensions.")."</b>";

      $display.= back_to_main();
      return ($display);
    }

    /* Show tab dialog headers */
    if ($this->parent !== NULL){
      if ($this->is_account){
        $display= $this->show_disable_header(_("Remove SCALIX account"),
            _("This account has SCALIX synchronization enabled. You can disable it by clicking below."));
      } else {
        $display= $this->show_enable_header(_("Create SCALIX account"), _("This account has SCALIX synchronization disabled. You can enable it by clicking below."));
        return ($display);
      }
    }

    /* Trigger forward add dialog? */
    if (isset($_POST['add_local_forwarder'])){
      $this->forward_dialog= TRUE;
      $this->dialog= TRUE;
    }

    /* Cancel forward add dialog? */
    if (isset($_POST['add_locals_cancel'])){
      $this->forward_dialog= FALSE;
      $this->dialog= FALSE;
    }


    $smarty->assign("mailboxClasses", $mailboxClasses);
    $smarty->assign("serverLanguages", $serverLanguages);
    foreach(array("perms", "scalixScalixObject", "scalixMailnode", "scalixAdministrator", "scalixMailboxAdministrator",
      "scalixServerLanguage", "scalixLimitMailboxSize", "scalixLimitOutboundMail", "scalixEmailAddress",
      "scalixLimitInboundMail", "scalixLimitNotifyUser", "scalixHideUserEntry", "scalixMailboxClass") as $val){

      $smarty->assign("$val", set_post($this->$val));
    }

    $tmp = $this->plInfo();
    foreach($tmp['plProvidedAcls'] as $name => $desc){
      $smarty->assign($name."ACL", $this->getacl($name));
    }

    /* Fill checkboxes */
    if ($this->scalixAdministrator) {
      $smarty->assign("scalixAdministrator", "checked");
    } else {
      $smarty->assign("scalixAdministrator", "");
    }
    if ($this->scalixMailboxAdministrator) {
      $smarty->assign("scalixMailboxAdministrator", "checked");
    } else {
      $smarty->assign("scalixMailboxAdministrator", "");
    }
    if ($this->scalixLimitOutboundMail) {
      $smarty->assign("scalixLimitOutboundMail", "checked");
    } else {
      $smarty->assign("scalixLimitOutboundMail", "");
    }
    if ($this->scalixLimitInboundMail) {
      $smarty->assign("scalixLimitInboundMail", "checked");
    } else {
      $smarty->assign("scalixLimitInboundMail", "");
    }
    if ($this->scalixLimitNotifyUser) {
      $smarty->assign("scalixLimitNotifyUser", "checked");
    } else {
      $smarty->assign("scalixLimitNotifyUser", "");
    }
    if ($this->scalixHideUserEntry) {
      $smarty->assign("scalixHideUserEntry", "checked");
    } else {
      $smarty->assign("scalixHideUserEntry", "");
    }

    $display.= $smarty->fetch (get_template_path('generic.tpl', TRUE, dirname(__FILE__)));
    return ($display);
  }


  /* remove object from parent */
  function remove_from_parent()
  {
    /* Cancel if there's nothing to do here */
    if (!$this->initially_was_account){
      return;
    }
    
    /* include global link_info */
    $ldap= $this->config->get_ldap_link();

    /* Remove and write to LDAP */
    plugin::remove_from_parent();

    /* Zero arrays */
    $this->attrs['scalixEmailAddress']= array();

    /* Unset fake boolean attributes from entry */
    foreach ($this->fakeBooleanAttributes as $val){
      $this->attrs["$val"]= array();
    }

    /*unset scalixScalixObject*/
    $this->attrs['scalixScalixObject']=array();

    @DEBUG (DEBUG_LDAP, __LINE__, __FUNCTION__, __FILE__,$this->attributes, "Save");
    $ldap->cd($this->dn);
    $ldap->modify($this->attrs);
    if (!$ldap->success()){
      msg_dialog::display(_("LDAP error"), msgPool::ldaperror($ldap->get_error(), $this->dn, LDAP_MOD, get_class()));
    }

    /* Optionally execute a command after we're done */
    $this->handle_post_events("remove");
  }


  /* Save data to object */
  function save_object()
  {
    if (isset($_POST['scalixTab'])){

      /* Save ldap attributes */
      plugin::save_object();

      /* Check if given value is not empty */
      if (isset($_POST['email_address']) && !empty($_POST['email_address']) && isset($_POST['add_email'])){
        $valid= FALSE;

        /* Valid mail address */
        if( ($this->is_template && !tests::is_email($_POST['email_address'], TRUE)) ||
            (!$this->is_template && !tests::is_email($_POST['email_address']))){
          msg_dialog::display(_("Error"), _("Cannot add invalid mail address!"), ERROR_DIALOG);
        }else{
          $valid = TRUE;
        }

        /* Check write access */
        if(!$this->acl_is_writeable("scalixEmailAddress")){
          msg_dialog::display(_("Error"), _("You have no permission to modify these addresses."), ERROR_DIALOG);
        }else{

          /* Mail address already in use ? */
          if ($valid && ($user= $this->addEmail (get_post('email_address'))) != ""){
            $ui= get_userinfo();
            if ($user != $ui->username){
              msg_dialog::display(_("Error"), sprintf(_("Cannot add mail address: it is already used by user '%s'."), $user),
                  ERROR_DIALOG);
            }
          }
        }
      }

      /* Delete email addresses */
      if (isset($_POST['delete_email']) && isset ($_POST['emails_list']) && $this->acl_is_writeable("scalixEmailAddress")){
        if(!$this->acl_is_writeable("scalixEmailAddress")){
          msg_dialog::display(_("Error"), _("You have no permission to modify these addresses."), ERROR_DIALOG);
        }else{
          if (count($_POST['emails_list'])){
            $this->delEmail ($_POST['emails_list']);
          }
        }
      }

      /* Save fakeBooleanAttributes*/
      foreach ($this->fakeBooleanAttributes as $val){
        if ($this->acl_is_writeable($val)) {
          if( isset ($_POST["$val"])) {
            $data = TRUE;
          } else {
            $data = FALSE;
          }
          if ($this->$val != $data){
            $this->is_modified= TRUE;
          }

          $this->$val= $data;
        }
      }
    }
  }


  /* Save data to LDAP, depending on is_account we save or delete */
  function save()
  {
    $ldap= $this->config->get_ldap_link();

    /* Call parents save to prepare $this->attrs */
    plugin::save();

    /* Save arrays */
    $this->attrs['scalixEmailAddress']= $this->scalixEmailAddress;    
    
    /* Save boolean vars ... the scalix schema expects them as strings */
    if(!$this->scalixGroup){
      $this->attrs['scalixAdministrator'] = $this->scalixAdministrator?"TRUE":"FALSE";
      $this->attrs['scalixMailboxAdministrator'] = $this->scalixMailboxAdministrator?"TRUE":"FALSE";
      $this->attrs['scalixLimitOutboundMail'] = $this->scalixLimitOutboundMail?"TRUE":"FALSE";
      $this->attrs['scalixLimitInboundMail'] = $this->scalixLimitInboundMail?"TRUE":"FALSE";
      $this->attrs['scalixLimitNotifyUser'] = $this->scalixLimitNotifyUser?"TRUE":"FALSE";
    }
    
    $this->attrs['scalixScalixObject'] = $this->scalixScalixObject?"TRUE":"FALSE";
    $this->attrs['scalixHideUserEntry'] = $this->scalixHideUserEntry?"TRUE":"FALSE";

    /* Remove Mailquota if = "" */
    if((isset($this->attrs['scalixLimitMailboxSize']))&&($this->attrs['scalixLimitMailboxSize']=="")) {
      $this->attrs['scalixLimitMailboxSize']=array();
    }

    /* Save data to LDAP */
    $ldap->cd($this->dn);
    $ldap->modify($this->attrs);
    if (!$ldap->success()){
      msg_dialog::display(_("LDAP error"), msgPool::ldaperror($ldap->get_error(), $this->dn, LDAP_MOD, get_class()));
    }

    /* Optionally execute a command after we're done */
    if ($this->initially_was_account == $this->is_account){
      if ($this->is_modified){
        $this->handle_post_events("modify");
      }
    } else {
      $this->handle_post_events("add");
    }

  }

  /* Check formular input */
  function check()
  {
    $ldap= $this->config->get_ldap_link();

    $message= array();

    if(empty($this->scalixMailnode)){
      $message[]= msgPool::required(_("Mail node"));
    }
    if(empty($this->scalixScalixObject)){
      $message[]= msgPool::required(_("SCALIX object"));
    }

    $ldap->cd($this->config->current['BASE']);
    foreach($this->scalixEmailAddress as $k => $mail) {    	
      $ldap->search ("(&(!(objectClass=gosaUserTemplate))(objectClass=scalixUserClass)(scalixEmailAddress=".
        $mail.")(!(uid=".$this->uid."))(!(cn=".$this->uid.")))", array("uid"));
  
      if ($ldap->count() != 0){
        $message[]= msgPool::duplicated(_("Email address"));
      }
    }

    /* Check quota */
    if ($this->scalixLimitMailboxSize != '' && $this->acl_is_writeable("scalixLimitMailboxSize")){
      if (!is_numeric($this->scalixLimitMailboxSize)) {
        $message[]= msgPool::invalid(_("Limit Mailbox"));
      } else {
        $this->scalixLimitMailboxSize= (int) $this->scalixLimitMailboxSize;
      }
    }

    return ($message);
  }

  /* Adapt from template, using 'dn' */
  function adapt_from_template($dn, $skip= array())
  {
    plugin::adapt_from_template($dn, $skip);

    foreach (array("scalixEmailAddress") as $val){

      if (in_array_strict($val, $skip)){
        continue;
      }

      $this->$val= array();
      if (isset($this->attrs["$val"]["count"])){
        for ($i= 0; $i<$this->attrs["$val"]["count"]; $i++){
          $value= $this->attrs["$val"][$i];
          foreach (array("sn", "givenName", "uid") as $repl){
            if (preg_match("/%$repl/i", $value)){
              $value= preg_replace ("/%$repl/i", $this->parent->$repl, $value);
            }
          }
          array_push($this->$val, strtolower(rewrite($value)));
        }
      }
    }
    $this->mail= strtolower(rewrite($this->mail));
  }

  function addEmail($address)
  {
    $ldap= $this->config->get_ldap_link();

    $address= strtolower($address);

    /* Is this address already assigned in LDAP? */
    $ldap->cd ($this->config->current['BASE']);
    $ldap->search ("(&(objectClass=scalixUserClass)(|(scalixEmailAddress=$address)".
        "(scalixEmailAddress=$address)))", array("uid"));

    if ($ldap->count() > 0){
      $attrs= $ldap->fetch ();
      return ($attrs["uid"][0]);
    }

    /* Add to list of alternates */
    if (!in_array_strict($address, $this->scalixEmailAddress)){
      $this->scalixEmailAddress[]= $address;
      $this->is_modified= TRUE;
    }

    //sort ($this->scalixEmailAddress); //scalix makes the first email the destination, all others can receive
    reset ($this->scalixEmailAddress);

    return ("");
  }


  function delEmail($addresses)
  {
    $this->scalixEmailAddress= array_remove_entries ($addresses, $this->scalixEmailAddress);
    $this->is_modified= TRUE;
  }


  static function plInfo()
  {
    return (array(
          "plDescription"     => _("SCALIX account"),
          "plSelfModify"      => TRUE,
          "plDepends"         => array("user"),
          "plPriority"        => 2,
          "plSection"         => array("personal" => _("My account")),
          "plCategory"        => array("users"),
          "plOptions"         => array(),

          "plRequirements"=> array(
              'ldapSchema' => array('scalixUserClass' => ''),
              'onFailureDisablePlugin' => array(get_class())
              ),

          "plProvidedAcls"  => array(

            "scalixMailnode"            => _("SCALIX Mail node"),
            "scalixMailboxClass"        => _("Mailbox class"),
            "scalixServerLanguage"      => _("Server language"),
            "scalixAdministrator"       => _("Administrator"),
            "scalixMailboxAdministrator"=> _("Mailbox administrator"),
            "scalixHideUserEntry"       => _("Hide user entry in SCALIX"),
            "scalixLimitMailboxSize"    => _("Mailbox size limitations"),
            "scalixLimitOutboundMail"   => _("Limit outbound"),
            "scalixLimitInboundMail"    => _("Limit inbound"),
            "scalixLimitNotifyUser"     => _("Notify user"),
            "scalixEmailAddress"        => _("SCALIX email addresses"))

            ));
  }


  function saveCopyDialog()
  {
    if (isset($_POST['scalixTab'])){

      /* Add email addresses */
      if (isset($_POST['add_email'])){

        /* Check if given value is not empty */
        if ($_POST['email_address'] != ""){
          $valid= FALSE;

          /* Valid mail address */
          if( ($this->is_template && !tests::is_email($_POST['email_address'], TRUE)) || 
              (!$this->is_template && !tests::is_email($_POST['email_address'])) ){
            msg_dialog::display(_("Error"), _("Cannot add invalid mail address!"), ERROR_DIALOG);
          }else{
            $valid = TRUE;
          }

          /* Mail address already in use ? */
          if ($valid && ($user= $this->addEmail (get_post('email_address'))) != ""){
            $ui= get_userinfo();
            if ($user != $ui->username){
              msg_dialog::display(_("Error"), sprintf(_("Cannot add mail address: it is already used by user '%s'."), $user), ERROR_DIALOG);
            }
          }
        }
      }

      /* Delete email addresses */
      if (isset($_POST['delete_email']) && isset ($_POST['emails_list'])){
        if (count($_POST['emails_list'])){
          $this->delEmail ($_POST['emails_list']);
        }
      }
    }
  }


  function PrepareForCopyPaste($source)
  {
    plugin::PrepareForCopyPaste($source);
    foreach ($this->fakeBooleanAttributes as $val){
      if (isset($source["$val"][0])&&$source["$val"][0]=="TRUE"){
        $this->$val = TRUE;
      } else {
        $this->$val = FALSE;
      }
    }

    /* Load attributes containing arrays */
    foreach (array("scalixEmailAddress") as $val){
      if (isset($source["$val"]["count"])){
        for ($i= 0; $i<$source["$val"]["count"]; $i++){
          array_push($this->$val, $source["$val"][$i]);
        }
      }
    }
  }


  /* Create the posix dialog part for copy & paste */
  function getCopyDialog()
  {
    $smarty = get_smarty(); 
    $smarty->assign("scalixEmailAddress",set_post($this->scalixEmailAddress));
    $str = $smarty->fetch(get_template_path("paste_generic.tpl",TRUE,dirname(__FILE__)));    
    $ret = array();
    $ret['string'] = $str;
    $ret['status'] = "";
    return($ret);
 
  }
}

// vim:tabstop=2:expandtab:shiftwidth=2:filetype=php:syntax:ruler
?>
