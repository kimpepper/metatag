<?php /**
 * @file
 * Contains \Drupal\metatag\Controller\DefaultController.
 */

namespace Drupal\metatag\Controller;

use Drupal\Component\Utility\String;
use Drupal\Core\Controller\ControllerBase;

/**
 * Default controller for the metatag module.
 */
class DefaultController extends ControllerBase {

  /**
   * Provides the metatag overview page.
   *
   * @return string
   *
   */
  public function overview() {
    // @FIXME: Most CTools APIs have moved into core. For more information, see https://www.drupal.org/node/2164623
// ctools_include('export');


    \Drupal::moduleHandler()->loadInclude('metatag', 'inc', 'metatag.admin');

    $metatags = metatag_get_info('tags');

    // @FIXME: The CTools Export API has into core in the form of exportable configuration and content entities. For more information, see https://www.drupal.org/developing/api/entity
// $configs = ctools_export_crud_load_all('metatag_config');

    ksort($configs);
    //uasort($configs, '_metatag_config_sort');

    $rows = array();
    foreach ($configs as $config) {
      $row = array();

      // Style disabled configurations differently.
      if (!empty($config->disabled)) {
        $row['class'][] = 'disabled';
      }

      $details = array();
      $details[] = array(
        '#type' => 'inline_template',
        '#template' => '<div class="metatag-config-label collapsed"><a href="#" class="toggle-details">{{ label }}</a></div>',
        '#context' => array(
          'label' => metatag_config_instance_label($config->instance),
        ),
      );

      $details[] = array('#markup' => '<div class="metatag-config-details js-hide">');

      $inherits = array();
      $parents = metatag_config_get_parent_instances($config->instance);
      array_shift($parents);
      foreach (array_reverse($parents) as $parent) {
        if (!isset($configs[$parent])) {
          $rows[$parent] = array(
            _metatag_config_overview_indent('<div class="metatag-config-label">' . String::checkPlain(metatag_config_instance_label($parent)) . '</div>', $parent),
            '',
          );
        }
        else {
          $inherits[$parent] = metatag_config_instance_label($parent);
          if (!empty($configs[$parent]->disabled)) {
            $inherits[$parent] .= ' ' . $this->t('(disabled)');
          }
        }
      }

      // Show how this config inherits from its parents.
      if (!empty($inherits)) {
        $details .= '<div class="inheritance"><p>' . $this->t('Inherits meta tags from: @parents', array('@parents' => implode(', ', $inherits))) . '</p></div>';
      }

      // Add a summary of the configuration's defaults.
      $summary = array();
      foreach ($config->config as $metatag => $data) {
        // Skip meta tags that were disabled.
        if (empty($metatags[$metatag])) {
          continue;
        }
        $summary[] = array(
          String::checkPlain($metatags[$metatag]['label']) . ':',
          String::checkPlain(metatag_get_value($metatag, $data, array('raw' => TRUE))),
        );
      }
      if (!empty($summary)) {
        $table = array(
          '#type' => 'table',
          '#rows' => $summary,
          '#attributes' => array('class' => array('metatag-value-summary')),
        );
        $details .= drupal_render($table);
      }
      else {
        $details .= '<p class="warning">No overridden default meta tags</p>';
        $row['class'][] = 'warning';
      }

      // Close the details div
      $details .= '</div>';

      // Add indentation to the leading cell based on how many parents the config has.
      $details = _metatag_config_overview_indent($details, $config->instance);

      $row['data']['details'] = $details;

      $operations = array();
      if (metatag_config_access('disable', $config)) {
        $operations['edit'] = array(
          'title' => ($config->export_type & EXPORT_IN_DATABASE) ?$this->t('Edit') :$this->t('Override'),
          'href' => 'admin/config/search/metatags/config/' . $config->instance,
        );
      }
      if (metatag_config_access('enable', $config)) {
        $operations['enable'] = array(
          'title' =>$this->t('Enable'),
          'href' => 'admin/config/search/metatags/config/' . $config->instance . '/enable',
          'query' => drupal_get_destination(),
        );
      }
      if (metatag_config_access('disable', $config)) {
        $operations['disable'] = array(
          'title' =>$this->t('Disable'),
          'href' => 'admin/config/search/metatags/config/' . $config->instance . '/disable',
          'query' => drupal_get_destination(),
        );
      }
      if (metatag_config_access('revert', $config)) {
        $operations['revert'] = array(
          'title' =>$this->t('Revert'),
          'href' => 'admin/config/search/metatags/config/' . $config->instance . '/revert',
        );
      }
      if (metatag_config_access('delete', $config)) {
        $operations['delete'] = array(
          'title' =>$this->t('Delete'),
          'href' => 'admin/config/search/metatags/config/' . $config->instance . '/delete',
        );
      }
      $operations['export'] = array(
        'title' =>$this->t('Export'),
        'href' => 'admin/config/search/metatags/config/' . $config->instance . '/export',
      );
      $row['data']['operations'] = array(
        'data' => array(
          '#theme' => 'links',
          '#links' => $operations,
          '#attributes' => array('class' => array('links', 'inline')),
        ),
      );

      $rows[$config->instance] = $row;
    }

    $build['config_table'] = array(
      '#theme' => 'table',
      '#header' => array(
        'type' =>$this->t('Type'),
        'operations' =>$this->t('Operations'),
      ),
      '#rows' => $rows,
      '#empty' =>$this->t('No meta tag defaults available yet.'),
      '#attributes' => array(
        'class' => array('metatag-config-overview'),
      ),
      '#attached' => array(
        'js' => array(
          drupal_get_path('module', 'metatag') . '/metatag.admin.js',
        ),
        'css' => array(
          drupal_get_path('module', 'metatag') . '/metatag.admin.css',
        ),
      ),
    );

    return $build;
  }
}
