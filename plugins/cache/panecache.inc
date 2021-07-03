<?php

/**
 * @file
 * Provides a time-based caching with custom cache id string for panel panes.
 */

// Plugin definition.
$plugin = array(
  'title' => 'Professional pane cache',
  'description' => 'Use rules and Cache Actions to determine when to clear the cache.',
  'cache get' => 'gang_cache_rules_cache_get_cache',
  'cache set' => 'gang_cache_rules_cache_set_cache',
  'cache clear' => 'gang_cache_rules_cache_clear_cache',
  'cache clear pane' => 'gang_cache_rules_cache_clear_pane_cache',
  'settings form' => 'gang_cache_rules_cache_settings_form',
  'settings form validate' => 'gang_cache_rules_cache_settings_form_validate',
  'settings form submit' => 'gang_cache_rules_cache_settings_form_submit',
  'defaults' => array(
    'lifetime' => 'default',
    'granularity' => 'none',
    'language' => 0,
    'language_content' => 0,
    'cache_key' => '',
    'substitute' => 1,
  ),
);

/**
 * Get cached content.
 */
function gang_cache_rules_cache_get_cache($conf, $display, $args, $contexts, $pane = NULL) {
  if ($conf['disable']) return;
  if (!empty($conf['new'])) {
    $cid = gang_cache_rules_cache_get_id($conf, $display, $args, $contexts, $pane);
    $cache = cache_get($cid, 'cache_panels');
    if (!$cache) {
      return FALSE;
    }
    if ((time() - $cache->created) > $conf['lifetime']) {
      return FALSE;
    }

    return $cache->data;
  }
  $message = 'The panel pane %pane is using the old cache actions caching mechanism,
      and won\'t be cached until you resave it.';
  $parameters = array('%pane' => $pane->subtype);
  watchdog('gang_cache', $message, $parameters, WATCHDOG_WARNING);
  // Be very verbose for people who can control panels.
  if (user_access('use panels caching features') && user_access('administer panels layouts')) {
    drupal_set_message(t($message, $parameters), 'warning');
  }
}

/**
 * Set cached content.
 */
function gang_cache_rules_cache_set_cache($conf, $content, $display, $args, $contexts, $pane = NULL) {
  if ($conf['disable']) return;
  if (!empty($conf['new'])) {
    $lifetime = $conf['lifetime'] == 'custom' ? $conf['lifetime_custom'] : $conf['lifetime'] == 'default' ? variable_get('panels_gang_cache_default', 0) : $conf['lifetime']; //set default in admin ui, default to permanent
    $cid = gang_cache_rules_cache_get_id($conf, $display, $args, $contexts, $pane);
    if($content->content->module == 'views') { 
      //views panes get too big object cached altogether with html output, get rid of it.
      unset($content->content->content['#views_contextual_links_info']);
      unset($content->content->content['#contextual_links']);
    }
    if($lifetime > 0) cache_set($cid, $content, 'cache_panels', REQUEST_TIME + $lifetime);
    if($lifetime == 0) cache_set($cid, $content, 'cache_panels', CACHE_PERMANENT);
    if($lifetime == -1) cache_set($cid, $content, 'cache_panels', CACHE_TEMPORARY);
  }
}

/**
 * Clear cached content.
 *
 * @param stdClass $display
 *   The display object. If this object has a property named clear_pane, then
 *   that pane will be cleared.
 */
function gang_cache_rules_cache_clear_cache($display) {
  $cid = 'rc';
  // Add some text if this is a pane, so that we don't clear all panes when
  // we clear a panel pane.
  if (isset($display->clear_pane)) {
    $cid .= ':p';
  }
  if (is_numeric($display->did) && $display->did) {
    $cid .= ':' . $display->did;
  }
  else {
    // Add the cache key if this is an in-code display.
    $cid .= ':' . $display->cache_key;
  }
  // If this is a mini panel then we have the owner property. Let's
  // use the machine name for those.
  if (isset($display->owner->name)) {
    $cid .= ':' . $display->owner->name;
  }
  // If we have a pane specified, then append that to the key as well.
  if (isset($display->clear_pane)) {
    $cid .= ':' . $display->clear_pane->pid;
  }
  cache_clear_all($cid, 'cache', TRUE);
}

/**
 * Figure out an id for our cache based upon input and settings.
 */
function gang_cache_rules_cache_get_id($conf, $display, $args, $contexts, $pane) {
  $id = $conf['cache_key'] . '.';
  if (!empty($display->context) && !empty($conf['substitute'])) {
    $id = ctools_context_keyword_substitute($id, array(), $display->context);
  }

  // If the panel is stored in the database it'll have a numeric did value.
  if (is_numeric($display->did)) {
    $id .= ':' . $display->did;
  }
  // Exported panels won't have a numeric did but may have a usable cache_key.
  else if (!empty($display->cache_key)) {
    $id .= ':' . str_replace('panel_context:', '', $display->cache_key);
  }
  // Alternatively use the css_id.
  else if (!empty($display->css_id)) {
    $id .= ':' . $display->css_id;
  }
  // Failover to just appending the did, which may be the completely unusable
  // string 'new'.
  else {
    $id .= ':' . $display->did;
  }

  if ($pane) {
    $id .= ':' . $pane->pid;
  }

if (user_access('view pane admin links')) {
    $id .= ':adm';
  }
  switch ($conf['granularity']) {
    case 'args':
      foreach ($args as $arg) {
        // Arguments can be many things, but we only consider the ones that are
        // string values.
        if (is_string($arg)) {
          $id .= ':' . $arg;
        }
      }
      break;

    case 'context':
      if (!is_array($contexts)) {
        $contexts = array($contexts);
      }
      foreach ($contexts as $context) {
        if (isset($context->argument)) {
          $id .= ':' . $context->argument;
        }
      }
      break;
  }
  // Add cache for individual language.
  if ($conf['language']) {
    global $language;
    $id .= ':' . $language->language;
  }
  // Add cache for individual content language.
  if (!empty($conf['language_content'])) {
    global $language_content;
    $id .= ':' . $language_content->language;
  }
  if(!empty($pane->configuration['use_pager']) && !empty($_GET['page'])) {
    $id .= ':p' . check_plain($_GET['page']);
  }
  $cache = new stdClass();
  $cache->conf = $conf;
  $cache->display = $display;
  $cache->args = $args;
  $cache->contexts = $contexts;
  $cache->pane = $pane;
  $cache->key = $id;

  // Let other modules alter the cache key.
  drupal_alter('gang_cache_panels_cache_key', $cache);
  return $cache->key;
}

/**
 * Provide a form for the cache settings.
 *
 * @param array $conf
 *   The current configuration.
 * @param stdClass $display
 *   The display object
 * @param string $pid
 *   The pane pid.
 */
function gang_cache_rules_cache_settings_form(&$conf, $display, $pid) {
  $form = array();

  $options = drupal_map_assoc(array(60, 300, 1800, 3600, 21600, 86400, 259200, 604800, 1209600, 2592000, 7776000, 31536000), 'format_interval');
  $options = array(0 => 'Permanent cache', -1 => 'Temporary cache', 'default' => 'Default (set in admin ui)') + $options + array('custom' => 'Custom');
  $lifetime = $conf['lifetime'] == 'custom' ? ': ' . $conf['lifetime_custom'] : $conf['lifetime'] == 'default' ? ': ' . variable_get('panels_gang_cache_default', 0) : ''; //set default in admin ui, default to permanent
  $form['lifetime'] = array(
    '#title' => 'Lifetime: ' . $conf['lifetime'] . $lifetime,
    '#type' => 'select',
    '#options' => $options,
    '#default_value' => $conf['lifetime'],
    '#description' => 'Set default at https://hima-layas.com/en/admin/config/system/gang/variables.',
  );
  $form['lifetime_custom'] = array(
    '#type' => 'numberfield',
    '#title' => 'Custom time.',
    '#min' => 1,
    '#description' => 'Cache duration time in seconds.',
    '#default_value' => $conf['lifetime_custom'],
    '#process' => array('ctools_dependent_process'),
    '#dependency' => array(
      'edit-settings-lifetime' => array('custom'),
    ),
  );

  $form['advanced'] = array(
    '#type' => 'fieldset',
    '#title' => 'Advanced',
    '#collapsible' => TRUE,
    '#collapsed' => FALSE,
  );

  $form['advanced']['cache_key'] = array(
    '#type' => 'textarea',
    '#required' => TRUE,
	'#rows'=> 2,
    '#title' => 'Cache key',
    '#default_value' => $conf['cache_key'],
    '#description' => 'This is the key that will be used.
      The granularity and language settings will be appended after this key.
      You can use substitutions here.',
  );
  $form['advanced']['substitute'] = array(
    '#type' => 'checkbox',
    '#title' => 'Use context keywords',
    '#description' => 'If checked, context keywords will be substituted in this content.
      This is not compatible with the standard cache actions clear pane action!',
    '#default_value' => $conf['substitute'],
  );
  $form['advanced']['contexts'] = array(
    '#title' => 'Substitutions',
    '#type' => 'fieldset',
    '#collapsible' => TRUE,
    '#collapsed' => TRUE,
  );
  $form['granularity'] = array(
    '#title' => 'Granularity',
    '#type' => 'select',
    '#options' => array(
      'args' => 'Arguments',
      'context' => 'Context',
      'none' => 'None',
    ),
    '#description' => 'If "arguments" are selected, this content will be cached per individual argument to the entire display; if "contexts" are selected, this content will be cached per unique context in the pane or display; if "neither" there will be only one cache for this pane.',
    '#default_value' => $conf['granularity'],
  );
  $form['language'] = array(
    '#title' => 'Cache per interface language',
    '#type' => 'checkbox',
    '#default_value' => $conf['language'],
    '#description' => 'Select this if you want to cache content per language',
  );
  $form['language_content'] = array(
    '#title' => 'Cache per content language',
    '#type' => 'checkbox',
    '#default_value' => $conf['language_content'],
    '#description' => 'Select this if you want to cache content per language',
  );
  $form['disable'] = array(
    '#type' => 'checkbox',
    '#title' => 'Disable',
    '#default_value' => $conf['disable'],
    '#weight' => -10,
  );
  $rows = array();

  foreach ($display->context as $context) {
    foreach (ctools_context_get_converters('%' . check_plain($context->keyword) . ':', $context) as $keyword => $title) {
      $rows[] = array(
        check_plain($keyword),
        '@identifier: @title', array('@title' => $title, '@identifier' => $context->identifier),
      );
    }
  }
  $header = array('Keyword', 'Value');
  $form['advanced']['contexts']['context'] = array('#markup' => theme('table', array('header' => $header, 'rows' => $rows)));
  // Store the old cache key so we can look up any rules that needs changing.
  $form['old_cache_key'] = array(
    '#type' => 'value',
    '#value' => $conf['cache_key'],
  );
  // Store the display cache key so we can use it in the submit callback.
  $form['display_cache_key'] = array(
    '#type' => 'value',
    '#value' => $display->cache_key,
  );
  return $form;
}

/**
 * Validates the cache settings.
 */
function gang_cache_rules_cache_settings_form_validate($form, $values) {
  $args = func_get_args();
  $display = $form['display']['#value'];
  $pid = $form['pid']['#value'];
/*  foreach ($display->content as $pane) {
    if (!empty($pane->cache['method']) && $pane->cache['method'] == 'rules' && $pid != $pane->pid && $pane->cache['settings']['cache_key'] == $values['advanced']['cache_key']) {
      form_set_error('cache_key', 'The cache key must be unique');
    }
  }*///we append unique key later
}

/**
 * Saves the cache settings.
 *
 * @param array $values
 *   The values to store.
 */
function gang_cache_rules_cache_settings_form_submit(&$values) {
  $values['cache_key'] = $values['advanced']['cache_key'];
  $values['substitute'] = $values['advanced']['substitute'];
  // Add an indicator that this is stored with the new caching mechanism.
  $values['new'] = TRUE;

  // Store cache key updates in a variable,
  // so that we can update any rules that may be using the old cache key.
  if (!empty($values['old_cache_key']) && $values['old_cache_key'] != $values['cache_key']) {
    $cache_actions_updated_panes = variable_get('cache_actions_updated_panes', array());
    // Store the oldest cache key for this session under the newest key.
    if (isset($cache_actions_updated_panes[$values['display_cache_key']][$values['old_cache_key']])) {
      $cache_actions_updated_panes[$values['display_cache_key']][$values['cache_key']] = $cache_actions_updated_panes[$values['display_cache_key']][$values['old_cache_key']];
      unset($cache_actions_updated_panes[$values['display_cache_key']][$values['old_cache_key']]);
    }
    else {
      $cache_actions_updated_panes[$values['display_cache_key']][$values['cache_key']] = $values['old_cache_key'];
    }
    variable_set('cache_actions_updated_panes', $cache_actions_updated_panes);
  }
  unset($values['advanced']['cache_key'], $values['advanced']['substitute'], $values['old_cache_key'], $values['display_cache_key']);
}
