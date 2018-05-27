<?php
/**
 * @file
 * Theme functions for the Theme Tweaks module.
 */

/**
 * Returns HTML for a theme's color form.
 *
 * @param $variables
 *   An associative array containing:
 *   - form: A render element representing the form.
 *
 * @ingroup themeable
 */
function theme_theme_tweaks_scheme_form($variables) {
  $form = $variables['form'];

  $output  = '';
  $output .= '<div class="color-form clearfix">';
  $output .= '<div id="palette" class="clearfix">';
  // Color schemes
  $output .= backdrop_render($form['scheme']);
  // Palette
  foreach (element_children($form['palette']) as $name) {
    $output .= backdrop_render($form['palette'][$name]);
  }
  $output .= '</div>';
  // Preview
  $output .= backdrop_render_children($form);
  $output .= '<div class="color-preview">';
  $output .= '<h2>' . t('Preview') . '</h2>';
  // Attempt to load preview HTML if the theme provides it.
  $output .= '<iframe style="width:100%; height:50vh; transform: scale(0.75); transform-origin: left top;" src="' . url('<front>') . '"></iframe>';
  // Close the wrapper divs.
  $output .= '</div>';
  $output .= '</div>';

  return $output;
}

