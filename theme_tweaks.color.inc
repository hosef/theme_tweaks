<?php

function theme_tweaks_colors($form, $form_state, $theme) {
  $base = backdrop_get_path('module', 'theme_tweaks');
  $info = theme_tweaks_get_info($theme);

  $info['schemes'][''] = array('title' => t('Custom'), 'colors' => array());
  $color_sets = array();
  $schemes = array();
  foreach ($info['schemes'] as $key => $scheme) {
    $color_sets[$key] = $scheme['title'];
    $schemes[$key] = $scheme['colors'];
    $schemes[$key] += $info['schemes']['default']['colors'];
  }

  // See if we're using a predefined scheme.
  // Note: we use the original theme when the default scheme is chosen.
  $current_scheme = theme_get_setting('color.palette', $theme);
  foreach ($schemes as $key => $scheme) {
    if ($current_scheme == $scheme) {
      $scheme_name = $key;
      break;
    }
  }
  if (empty($scheme_name)) {
    if (empty($current_scheme)) {
      $scheme_name = 'default';
    }
    else {
      $scheme_name = '';
    }
  }

  $form['color'] = array(
    '#type' => 'fieldset',
    '#title' => t('Color scheme'),
    '#weight' => -1,
    '#attributes' => array('id' => 'theme_tweaks_scheme_form'),
    '#theme' => 'theme_tweaks_scheme_form',
  );

  // Add scheme selector.
  $form['color']['scheme'] = array(
    '#type' => 'select',
    '#title' => t('Color set'),
    '#options' => $color_sets,
    '#default_value' => $scheme_name,
    '#attached' => array(
      // Add Farbtastic color picker.
      'library' => array(
        array('system', 'farbtastic'),
      ),
      // Add custom CSS.
      'css' => array(
        $base . '/css/theme_tweaks.admin.css' => array(),
      ),
      // Add custom JavaScript.
      'js' => array(
        $base . '/js/theme_tweaks.js',
        array(
          'data' => array(
            'color' => array(
              'reference' => theme_tweaks_get_palette($theme, TRUE),
              'schemes' => $schemes,
            ),
          ),
          'type' => 'setting',
        ),
      ),
    ),
  );

  // Add palette fields.
  $palette = theme_tweaks_get_palette($theme);
  $names = $info['fields'];
  $form['color']['palette']['#tree'] = TRUE;
  foreach ($palette as $name => $value) {
    if (isset($names[$name])) {
      $form['color']['palette'][$name] = array(
        '#type' => 'textfield',
        '#title' => check_plain($names[$name]),
        '#value_callback' => 'theme_tweaks_palette_color_value',
        '#default_value' => $value,
        '#size' => 8,
      );
    }
  }
  $form['theme'] = array('#type' => 'value', '#value' => $theme);
  $form['info'] = array('#type' => 'value', '#value' => $info);

  // Add validate callback.
  $form['#validate'][] = 'theme_tweaks_scheme_form_validate';
  // Add submit callback.
  $form['#submit'][] = 'theme_tweaks_scheme_form_submit';

  $form['actions'] = array('#type' => 'actions');
  $form['actions']['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Save colors'),
  );

  return $form;
}

/**
 * Form validation handler for theme_tweaks_scheme_form().
 *
 * @see theme_tweaks_scheme_form_submit()
 */
function theme_tweaks_scheme_form_validate($form, &$form_state) {
  // Only accept hexadecimal CSS color strings to avoid XSS upon use.
  foreach ($form_state['values']['palette'] as $key => $color) {
    if (!theme_tweaks_valid_hexadecimal_string($color)) {
      form_set_error('palette][' . $key, t('You must enter a valid hexadecimal color value for %name.', array('%name' => $form['color']['palette'][$key]['#title'])));
    }
  }
}

/**
 * Form submission handler for theme_tweaks_scheme_form().
 *
 * @see theme_tweaks_scheme_form_validate()
 */
function theme_tweaks_scheme_form_submit($form, &$form_state) {
  // Get theme coloring info.
  if (!isset($form_state['values']['info'])) {
    return;
  }
  $theme = $form_state['values']['theme'];
  $info = $form_state['values']['info'];

  // Remove color info so it doesn't get saved in config.
  unset($form_state['values']['info']);

  // Resolve palette.
  $palette = $form_state['values']['palette'];
  if ($form_state['values']['scheme'] != '') {
    foreach ($palette as $key => $color) {
      if (isset($info['schemes'][$form_state['values']['scheme']]['colors'][$key])) {
        $palette[$key] = $info['schemes'][$form_state['values']['scheme']]['colors'][$key];
      }
    }
    $palette += $info['schemes']['default']['colors'];
  }

  // Make sure enough memory is available, if PHP's memory limit is compiled in.
  if (function_exists('memory_get_usage')) {
    // Fetch source image dimensions.
    $source = backdrop_get_path('theme', $theme) . '/' . $info['base_image'];
    list($width, $height) = getimagesize($source);

    // We need at least a copy of the source and a target buffer of the same
    // size (both at 32bpp).
    $required = $width * $height * 8;
    // We intend to prevent color scheme changes if there isn't enough memory
    // available.  memory_get_usage(TRUE) returns a more accurate number than
    // memory_get_usage(), therefore we won't inadvertently reject a color
    // scheme change based on a faulty memory calculation.
    $usage = memory_get_usage(TRUE);
    $memory_limit = ini_get('memory_limit');
    $size = parse_size($memory_limit);
    if (!backdrop_check_memory_limit($usage + $required, $memory_limit)) {
      backdrop_set_message(t('There is not enough memory available to PHP to change this theme\'s color scheme. You need at least %size more. Check the <a href="@url">PHP documentation</a> for more information.', array('%size' => format_size($usage + $required - $size), '@url' => 'http://www.php.net/manual/ini.core.php#ini.sect.resource-limits')), 'error');
      return;
    }
  }

  // Delete old files.
  $files = theme_get_setting('color.files');
  if ($files) {
    foreach ($files as $file) {
      @backdrop_unlink($file);
    }
  }
  if (isset($file) && $file = dirname($file)) {
    @backdrop_rmdir($file);
  }

  // Don't render the default colorscheme, use the standard theme instead.
  if (implode(',', theme_tweaks_get_palette($theme, TRUE)) == implode(',', $palette)) {
    unset($form_state['values']['scheme']);
    unset($form_state['values']['palette']);
    $form_state['values']['color'] = NULL;
    return;
  }

  // Prepare target locations for generated files.
  $id = $theme . '-' . substr(hash('sha256', serialize($palette) . microtime()), 0, 8);
  $paths['color'] = 'public://color';
  $paths['target'] = $paths['color'] . '/' . $id;
  foreach ($paths as $path) {
    file_prepare_directory($path, FILE_CREATE_DIRECTORY);
  }
  $paths['target'] = $paths['target'] . '/';
  $paths['id'] = $id;
  $paths['source'] = backdrop_get_path('theme', $theme) . '/';
  $paths['files'] = $paths['map'] = array();

  // Copy over neutral images.
  foreach ($info['copy'] as $file) {
    $base = backdrop_basename($file);
    $source = $paths['source'] . $file;
    $filepath = file_unmanaged_copy($source, $paths['target'] . $base);
    $paths['map'][$file] = $base;
    $paths['files'][] = $filepath;
  }

  // Rewrite theme stylesheets.
  $css = array();
  foreach ($info['css'] as $stylesheet) {
    // Build a temporary array with CSS files.
    $files = array();
    if (file_exists($paths['source'] . $stylesheet)) {
      $files[] = $stylesheet;
    }

    foreach ($files as $file) {
      // Aggregate @imports recursively for each configured top level CSS file
      // without optimization. Aggregation and optimization will be
      // handled by backdrop_build_css_cache() only.
      $style = backdrop_load_stylesheet($paths['source'] . $file, FALSE);

      // Return the path to where this CSS file originated from, stripping
      // off the name of the file at the end of the path.
      $base = base_path() . dirname($paths['source'] . $file) . '/';
      _backdrop_build_css_path(NULL, $base);

      // Prefix all paths within this CSS file, ignoring absolute paths.
      $style = preg_replace_callback('/url\([\'"]?(?![a-z]+:|\/+)([^\'")]+)[\'"]?\)/i', '_backdrop_build_css_path', $style);

      // Rewrite stylesheet with new colors.
      $style = _theme_tweaks_rewrite_stylesheet($theme, $info, $paths, $palette, $style);
      $base_file = backdrop_basename($file);
      $css[] = $paths['target'] . $base_file;
      _theme_tweaks_save_stylesheet($paths['target'] . $base_file, $style, $paths);
    }
  }

  // Save all the values into form state for saving to config.
  $form_state['values']['color'] = array(
    'palette' => $palette,
    'stylesheets' => $css,
    'files' => $paths['files'],
  );

  // Remove color field elements.
  unset($form_state['values']['scheme']);
  unset($form_state['values']['palette']);
}

function theme_tweaks_color_fields($form, $form_state, $theme) {
  dpm($theme);

  $form['function'] = array(
    '#markup' => __FUNCTION__
  );

  return $form;
}
