package navigator.services.screen;

// Interface for the screen service that accepts a string and displays it on the screen.
interface IScreenService {

  // displays a string on screen.
  void DisplayText(String s, int x, int y);
}
