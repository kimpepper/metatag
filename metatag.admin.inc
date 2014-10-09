<?php

/**
 * @file
 * Administration page callbacks for the metatag module.
 */

function _metatag_config_sort($a, $b) {
  $return = NULL;
  $a_contexts = explode(':', $a->instance);
  $b_contexts = explode(':', $b->instance);
  for ($i = 0; $i < max(count($a_contexts), count($b_contexts)); $i++) {
    $a_context = isset($a_contexts[$i]) ? $a_contexts[$i] : '';
    $b_context = isset($b_contexts[$i]) ? $b_contexts[$i] : '';
    if ($a_context == $b_context) {
      continue;
    }
    elseif ($a_context == 'global') {
      $return = -1;
    }
    elseif ($a_context == '') {
      $return = -1;
    }
    else {
      $return = strcmp($a_context, $b_context);
    }
  }
  return $return;
}

function _metatag_config_overview_indent($text, $instance) {
  $parents = metatag_config_get_parent_instances($instance);
  array_shift($parents);

  // Add indentation to the leading cell.
  if (!empty($parents)) {
    $prefix = array_fill(0, count($parents), '<div class="indent">');
    $suffix = array_fill(0, count($parents), '</div>');
    $text = implode('', $prefix) . $text . implode('', $suffix);
  }

  return $text;
}

/**
 * Build an FAPI #options array for the instance select field.
 */
function _metatag_config_instance_get_available_options() {
  $options = array();
  $instances = metatag_config_instance_info();

  foreach ($instances as $instance => $instance_info) {
    if (metatag_config_load($instance)) {
      continue;
    }
    $parents = metatag_config_get_parent_instances($instance, FALSE);
    array_shift($parents);
    if (!empty($parents)) {
      $parent = reset($parents);
      $parent_label = isset($instances[$parent]['label']) ? $instances[$parent]['label'] : t('Unknown');
      if (!isset($options[$parent_label])) {
        $options[$parent_label] = array();
        if (!metatag_config_load($parent)) {
          $options[$parent_label][$parent] = t('All');
        }
      }
      $options[$parent_label][$instance] = $instance_info['label'];
      unset($options[$parent]);
    }
    else {
      $options[$instance] = $instance_info['label'];
    }
  }

  return $options;
}

function metatag_config_add_form($form, &$form_state) {
  $form['instance'] = array(
    '#type' => 'select',
    '#title' => t('Type'),
    '#description' => t('Select the type of default meta tags you would like to add.'),
    '#options' => _metatag_config_instance_get_available_options(),
    '#required' => TRUE,
  );
  $form['config'] = array(
    '#type' => 'value',
    '#value' => array(),
  );

  $form['actions']['#type'] = 'actions';
  $form['actions']['save'] = array(
    '#type' => 'submit',
    '#value' => t('Add and configure'),
  );
  $form['actions']['cancel'] = array(
    '#type' => 'link',
    '#title' => t('Cancel'),
    '#href' => isset($_GET['destination']) ? $_GET['destination'] : 'admin/config/search/metatags',
  );

  return $form;
}

function metatag_config_add_form_submit($form, &$form_state) {
  form_state_values_clean($form_state);
  $config = (object) $form_state['values'];
  metatag_config_save($config);
  $form_state['redirect'] = 'admin/config/search/metatags/config/' . $config->instance;
}

function metatag_config_edit_form($form, &$form_state, $config) {
  $form['cid'] = array(
    '#type' => 'value',
    '#value' => !empty($config->cid) ? $config->cid : NULL,
  );
  $form['instance'] = array(
    '#type' => 'value',
    '#value' => $config->instance,
  );

  $contexts = explode(':', $config->instance);
  $options['context'] = $contexts[0];
  if ($contexts[0] != 'global') {
    // The context part of the instance may not map to an entity type, so allow
    // the token_get_entity_mapping() function to fallback to the provided type.
    if ($token_type = token_get_entity_mapping('entity', $contexts[0], TRUE)) {
      $options['token types'] = array($token_type);
    }
    else {
      $options['token types'] = array($contexts[0]);
    }
    // Allow hook_metatag_token_types_alter() to modify the defined tokens.
    \Drupal::moduleHandler()->alter('metatag_token_types', $options);
  }

  // Ensure that this configuration is properly compared to its parent 'default'
  // configuration values.
  if (count($contexts) > 1) {
    // If the config is something like 'node:article' or 'taxonomy_term:tags'
    // then the parent default config is 'node' or 'taxonomy_term'.
    $default_instance = $contexts;
    array_pop($default_instance);
    $default_instance = implode(':', $default_instance);
    $options['defaults'] = metatag_config_load_with_defaults($default_instance);
  }
  elseif ($contexts[0] != 'global') {
    // If the config is something like 'node' or 'taxonomy_term' then the
    // parent default config is 'global'.
    $options['defaults'] = metatag_config_load_with_defaults('global');
  }
  else {
    // If the config is 'global' than there are no parent defaults.
    $options['defaults'] = array();
  }

  metatag_metatags_form($form, $config->instance, $config->config, $options);
  $form['metatags']['#type'] = 'container';

  // Make the token browser available.
  $form['metatags']['#prefix'] = $form['metatags'][\Drupal\Core\Language\Language::LANGCODE_NOT_SPECIFIED]['#description'];
  unset($form['metatags'][\Drupal\Core\Language\Language::LANGCODE_NOT_SPECIFIED]['#description']);

  $form['actions']['#type'] = 'actions';
  $form['actions']['save'] = array(
    '#type' => 'submit',
    '#value' => t('Save'),
  );
  $form['actions']['cancel'] = array(
    '#type' => 'link',
    '#title' => t('Cancel'),
    '#href' => isset($_GET['destination']) ? $_GET['destination'] : 'admin/config/search/metatags',
  );

  $form['#submit'][] = 'metatag_config_edit_form_submit';
  return $form;
}

function metatag_config_edit_form_submit($form, &$form_state) {
  // Build the configuration object and save it.
  form_state_values_clean($form_state);
  $config = (object) $form_state['values'];
  // @todo Consider renaming the config field from 'config' to 'metatags'
  $config->config = $config->metatags[\Drupal\Core\Language\Language::LANGCODE_NOT_SPECIFIED];
  unset($config->metatags[\Drupal\Core\Language\Language::LANGCODE_NOT_SPECIFIED]);
  metatag_config_save($config);

  $label = metatag_config_instance_label($config->instance);
  drupal_set_message(t('The meta tag defaults for @label have been saved.', array('@label' => $label)));

  $form_state['redirect'] = 'admin/config/search/metatags';
}

function metatag_config_enable($config) {
  if (!isset($_GET['token']) || !drupal_valid_token($_GET['token'], 'enable-' . $config->instance)) {
    return MENU_ACCESS_DENIED;
  }

  // @FIXME: The CTools Export API has into core in the form of exportable configuration and content entities. For more information, see https://www.drupal.org/developing/api/entity
// ctools_export_crud_enable('metatag_config', $config);


  $label = metatag_config_instance_label($config->instance);
  drupal_set_message(t('The meta tag defaults for @label have been enabled.', array('@label' => $label)));
  drupal_goto();
}

function metatag_config_disable($config) {
  if (!isset($_GET['token']) || !drupal_valid_token($_GET['token'], 'disable-' . $config->instance)) {
    return MENU_ACCESS_DENIED;
  }

  // @FIXME: The CTools Export API has into core in the form of exportable configuration and content entities. For more information, see https://www.drupal.org/developing/api/entity
// ctools_export_crud_disable('metatag_config', $config);


  $label = metatag_config_instance_label($config->instance);
  drupal_set_message(t('The meta tag defaults for @label have been disabed.', array('@label' => $label)));
  drupal_goto();
}

function metatag_config_delete_form($form, &$form_state, $config) {
  $form['cid'] = array('#type' => 'value', '#value' => $config->cid);
  $form['instance'] = array('#type' => 'value', '#value' => $config->instance);

  $label = metatag_config_instance_label($config->instance);
  $delete = metatag_config_access('delete', $config);
  $title = $delete ? t('Are you sure you want to delete the meta tag defaults for @label?', array('@label' => $label)) : t('Are you sure you want to revert the meta tag defaults for @label?', array('@label' => $label));

  return confirm_form(
    $form,
    $title,
    'admin/config/search/metatags',
    t('This action cannot be undone.')
  );
}

function metatag_config_delete_form_submit($form, &$form_state) {
  $config = metatag_config_load($form_state['values']['instance']);
  metatag_config_delete($config->instance);

  $label = metatag_config_instance_label($config->instance);
  $delete = metatag_config_access('delete', $config);
  $title = $delete ? t('The meta tag defaults for @label have been deleted.', array('@label' => $label)) : t('The meta tag defaults for @label have been reverted.', array('@label' => $label));
  drupal_set_message($title);

  $form_state['redirect'] = 'admin/config/search/metatags';
}

function metatag_config_export_form($config) {
  // @FIXME: Most CTools APIs have moved into core. For more information, see https://www.drupal.org/node/2164623
// ctools_include('export');

  // @FIXME: The CTools Export API has into core in the form of exportable configuration and content entities. For more information, see https://www.drupal.org/developing/api/entity
// return drupal_get_form('ctools_export_form', ctools_export_crud_export('metatag_config', $config), t('Export'));

}

/**
 * Form constructor to revert nodes to their default metatags.
 *
 * @see metatag_bulk_revert_form_submit()
 * @ingroup forms
 */
function metatag_bulk_revert_form() {
  // Get the list of entity:bundle options
  $options = array();
  foreach (\Drupal::entityManager()->getDefinitions() as $entity_type => $entity_info) {
    foreach (array_keys($entity_info['bundles']) as $bundle) {
      if (metatag_entity_supports_metatags($entity_type, $bundle)) {
        $options[$entity_type . ':' . $bundle] =
          $entity_info['label'] . ': ' . $entity_info['bundles'][$bundle]['label'];
      }
    }
  }

  $form['update'] = array(
    '#type' => 'checkboxes',
    '#required' => TRUE,
    '#title' => t('Select the entities to revert'),
    '#options' => $options,
    '#default_value' => array(),
    '#description' => t('All meta tags will be removed for all content of the selected entities.'),
  );

  $form['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Revert'),
  );

  return $form;
}

/**
 * Form submit handler for metatag reset bulk revert form.
 *
 * @see metatag_batch_revert_form()
 * @see metatag_bulk_revert_batch_finished()
 */
function metatag_bulk_revert_form_submit($form, &$form_state) {
  $batch = array(
    'title' => t('Bulk updating metatags'),
    'operations' => array(),
    'finished' => 'metatag_bulk_revert_batch_finished',
    'file' => drupal_get_path('module', 'metatag') . '/metatag.admin.inc',
  );

  // Set a batch operation per entity:bundle.
  foreach (array_filter($form_state['values']['update']) as $option) {
    list($entity_type, $bundle) = explode(':', $option);
    $batch['operations'][] = array('metatag_bulk_revert_batch_operation', array($entity_type, $bundle));
  }

  batch_set($batch);
}

/**
 * Batch callback: delete custom metatags for selected bundles.
 */
function metatag_bulk_revert_batch_operation($entity_type, $bundle, &$context) {
  if (!isset($context['sandbox']['current'])) {
    $context['sandbox']['count'] = 0;
    $context['sandbox']['current'] = 0;
  }

  // Query the selected entity table.
  $entity_info = \Drupal::entityManager()->getDefinition($entity_type);
  $query = new EntityFieldQuery();
  $query->entityCondition('entity_type', $entity_type)
    ->propertyCondition($entity_info['entity keys']['id'], $context['sandbox']['current'], '>')
    ->propertyOrderBy($entity_info['entity keys']['id']);
  if ($entity_type != 'user') {
    /**
     * Entities which do not define a bundle such as User fail returning no results.
     * @see http://drupal.org/node/1054168#comment-5266208
     */
    $query->entityCondition('bundle', $bundle);
  }
  // Get the total amount of entities to process.
  if (!isset($context['sandbox']['total'])) {
    $context['sandbox']['total'] = $query->count()->execute();
    $query->count = FALSE;

    // If there are no bundles to revert, stop immediately.
    if (!$context['sandbox']['total']) {
      $context['finished'] = 1;
      return;
    }
  }

  // Process 25 entities per iteration.
  $query->range(0, 25);
  $result = $query->execute();
  $entity_ids = !empty($result[$entity_type]) ? array_keys($result[$entity_type]) : array();
  foreach ($entity_ids as $entity_id) {
    $metatags = metatag_metatags_load($entity_type, $entity_id);
    if (!empty($metatags)) {
      db_delete('metatag')->condition('entity_type', $entity_type)
        ->condition('entity_id', $entity_id)
        ->execute();
      metatag_metatags_cache_clear($entity_type, $entity_id);
      $context['results'][] = t('Reverted metatags for @bundle with id @id.', array(
        '@bundle' => $entity_type . ': ' . $bundle,
        '@id' => $entity_id,
      ));
    }
  }

  $context['sandbox']['count'] += count($entity_ids);
  $context['sandbox']['current'] = max($entity_ids);

  if ($context['sandbox']['count'] != $context['sandbox']['total']) {
    $context['finished'] = $context['sandbox']['count'] / $context['sandbox']['total'];
  }
}

/**
 * Batch finished callback.
 */
function metatag_bulk_revert_batch_finished($success, $results, $operations) {
  if ($success) {
    if (!count($results)) {
      drupal_set_message(t('No metatags were reverted.'));
    }
    else {
      $message = _theme('item_list', array('items' => $results));
      drupal_set_message($message);
    }
  }
  else {
    $error_operation = reset($operations);
    drupal_set_message(t('An error occurred while processing @operation with arguments : @args',
      array('@operation' => $error_operation[0], '@args' => print_r($error_operation[0], TRUE))));
  }
}

/**
 * Misc settings page.
 */
function metatag_admin_settings_form() {
  $form = array();

  $form['#attached'] = array(
    'js' => array(
      drupal_get_path('module', 'metatag') . '/metatag.admin.js',
    ),
    'css' => array(
      drupal_get_path('module', 'metatag') . '/metatag.admin.css',
    ),
  );

  $form['advanced'] = array(
    '#type' => 'fieldset',
    '#title' => t('Advanced settings'),
    '#collapsible' => TRUE,
    '#collapsed' => FALSE,
  );

  $form['advanced']['metatag_load_all_pages'] = array(
    '#type' => 'checkbox',
    '#title' => t('Output meta tags even if only global settings apply'),
    '#description' => t('By default Metatag will load the global default values for all pages that do not have meta tags assigned via the normal entity display or via Metatag Context. This may be disabled so that meta tags will only be output on pages that specifically have meta tags configured for them.'),
    '#default_value' => \Drupal::config('metatag.settings')->get('metatag_load_all_pages'),
  );

  $form['advanced']['metatag_tag_admin_pages'] = array(
    '#type' => 'checkbox',
    '#title' => t('Output meta tags on admin pages'),
    '#description' => t('By default meta tags will not be output on admin pages, but it may be beneficial for some sites to do so.'),
    '#default_value' => \Drupal::config('metatag.settings')->get('metatag_tag_admin_pages'),
  );

  $form['advanced']['metatag_extended_permissions'] = array(
    '#type' => 'checkbox',
    '#title' => t('Advanced permissions'),
    '#description' => t('Optionally add a permission for each individual meta tag. This provides tremendous flexibility for the editorial process, at the expense of making the permissions configuration more tedious.'),
    '#default_value' => \Drupal::config('metatag.settings')->get('metatag_extended_permissions'),
  );

  $form['advanced']['metatag_entity_no_lang_default'] = array(
    '#type' => 'checkbox',
    '#title' => t("Don't load entity's default language values if no languages match"),
    '#description' => t("On a site with multiple languages it is possible for an entity to not have meta tag values assigned for the language of the current page. By default the meta tags for an entity's default language value will be used in this scenario, with the canonical URL pointing to the URL. This option may be disabled, i.e. to only load meta tags for languages that specifically have them assigned, otherwise using defaults."),
    '#default_value' => \Drupal::config('metatag.settings')->get('metatag_entity_no_lang_default'),
  );

  $form['entities'] = array(
    '#type' => 'fieldset',
    '#title' => t('Master controls for all entities'),
    '#description' => t('By enabling and disabling items here, it is possible to control which entities (e.g. nodes, taxonomy terms) and bundles (e.g. content types, vocabularies) will have meta tags available to them.<br />Note: the entities first have to have meta tags enabled via hook_entity_info; see the API documentation for full details.'),
    '#collapsible' => TRUE,
    '#collapsed' => TRUE,
  );

  foreach (\Drupal::entityManager()->getDefinitions() as $entity_name => $entity_info) {
    // Only show entities that have been enabled via the hooks.
    if (!empty($entity_info['metatags'])) {
      $form['entities']['metatag_enable_' . $entity_name] = array(
        '#type' => 'checkbox',
        '#title' => t($entity_info['label']),
        '#default_value' => \Drupal::config('metatag.settings')->get('metatag_enable_' . $entity_name),
        '#description' => t('Enable meta tags for all pages this entity type.'),
      );

      if (!empty($entity_info['bundles'])) {
        $desc_added = FALSE;
        foreach ($entity_info['bundles'] as $bundle_name => $bundle_info) {
          // Some entities, e.g. User, (core) File, have a bundle with the same
          // name as the entity, so
          if ($bundle_name != $entity_name) {
            // Add an extra line to the description introducing the entity
            // bundles.
            if (!$desc_added) {
              $form['entities']['metatag_enable_' . $entity_name]['#disabled'] = TRUE;
              $form['entities']['metatag_enable_' . $entity_name]['#default_value'] = TRUE;
              $form['entities']['metatag_enable_' . $entity_name]['#description'] =  t('Each bundle for this entity must be controlled individually.');
              $desc_added = TRUE;
            }
            $form['entities']['metatag_enable_' . $entity_name . '__' . $bundle_name] = array(
              '#type' => 'checkbox',
              '#title' => t($bundle_info['label']),
              '#default_value' => \Drupal::config('metatag.settings')->get('metatag_enable_' . $entity_name . '__' . $bundle_name),
              '#attributes' => array(
                // Add some theming that'll indent this bundle.
                'class' => array('metatag-bundle-checkbox'),
              ),
            );
          }
        }
      }
    }
  }

  // Extra submission logic.
  $form['#submit'][] = 'metatag_admin_settings_form_submit';

  return system_settings_form($form);
}

/**
 * Form API submission callback for metatag_admin_settings_form().
 */
function metatag_admin_settings_form_submit() {
  cache_clear_all('entity_info:', 'cache', TRUE);
  cache_clear_all('*', 'cache_metatag', TRUE);
  drupal_set_message(t('The Metatag cache has been cleared, so all meta tags can be reloaded.'));
}
