#ifdef NDEBUG
#undef NDEBUG  // Assertions must stay active in Release CTest builds.
#endif
#include "../PocketJazzRadio/JazzNowPlaying.h"
#include <cassert>
#include <cstring>
#include <cstdio>

int main() {
  using pocketjazz::JazzNowPlaying;
  assert(JazzNowPlaying::hint("Miles Davis"));
  assert(JazzNowPlaying::hint("miles davis"));
  assert(JazzNowPlaying::hint("ELLA FITZGERALD"));
  static_assert(JazzNowPlaying::kCuratedCount == 595,
                "Only editorially curated artists may get previews");
  assert(JazzNowPlaying::hint("Billie Holiday"));
  assert(JazzNowPlaying::hint("Django Reinhardt"));
  assert(JazzNowPlaying::hint("Alice Coltrane"));
  assert(JazzNowPlaying::hint("Sun Ra"));
  assert(JazzNowPlaying::hint("Abdullah Ibrahim"));
  assert(JazzNowPlaying::hint("George Russell"));
  assert(JazzNowPlaying::hint("Renee Rosnes"));
  assert(JazzNowPlaying::hint("Charles McPherson"));
  assert(JazzNowPlaying::hint("Scott Joplin"));
  assert(JazzNowPlaying::hint("Artie Shaw"));
  assert(JazzNowPlaying::hint("Alberta Hunter"));
  assert(JazzNowPlaying::hint("Adelaide Hall"));
  assert(JazzNowPlaying::hint("Hank Mobley"));
  assert(JazzNowPlaying::hint("Johnny \"Hammond\" Smith"));
  assert(JazzNowPlaying::hint("Woody Shaw"));
  assert(JazzNowPlaying::hint("Melba Liston"));
  assert(JazzNowPlaying::hint("Cachao"));
  assert(JazzNowPlaying::hint("Jimmy Lyons"));
  assert(JazzNowPlaying::hint("Anke Helfrich"));
  assert(JazzNowPlaying::hint("Carmen Lundy"));
  assert(JazzNowPlaying::hint("Dorothy Ashby"));
  assert(JazzNowPlaying::hint("Giuseppi Logan"));
  assert(JazzNowPlaying::hint("Lionel Loueke"));
  assert(JazzNowPlaying::hint("Charly Antolini"));
  assert(JazzNowPlaying::hint("Margie Hyams"));
  assert(JazzNowPlaying::hint("Sibongile Khumalo"));
  assert(JazzNowPlaying::hint("Glauco Venier"));
  assert(JazzNowPlaying::hint("Attila Zoller"));
  assert(JazzNowPlaying::hint("Boyd Raeburn"));
  assert(JazzNowPlaying::hint("Irene Aebi"));
  assert(JazzNowPlaying::hint("Chico O'Farrill"));
  assert(JazzNowPlaying::hint("Alice Babs"));
  assert(JazzNowPlaying::hint("Bola Sete"));
  assert(JazzNowPlaying::hint("Nicole Mitchell"));
  assert(JazzNowPlaying::hint("Omer Simeon"));
  assert(JazzNowPlaying::hint("Cat Anderson"));
  assert(JazzNowPlaying::hint("Conte Candoli"));
  assert(JazzNowPlaying::hint("Miya Masaoka"));
  assert(JazzNowPlaying::hint("Stanley Cowell"));
  assert(JazzNowPlaying::hint("Esperanza Spalding"));
  assert(JazzNowPlaying::hint("Catherine Russell"));
  assert(JazzNowPlaying::hint("Buck Hill"));
  assert(JazzNowPlaying::hint("Gil Coggins"));
  assert(JazzNowPlaying::hint("Mike Westbrook"));
  assert(JazzNowPlaying::hint("Calvin Newborn"));
  assert(JazzNowPlaying::hint("Carlo Krahmer"));
  assert(JazzNowPlaying::hint("Cecil Scott"));
  assert(JazzNowPlaying::hint("Joe Farrell"));
  assert(JazzNowPlaying::hint("Thomas T. Dahl"));
  assert(JazzNowPlaying::hint("Mark Tokar"));
  assert(JazzNowPlaying::hint("Nathan Davis"));
  assert(JazzNowPlaying::hint("Fred Guy"));
  assert(JazzNowPlaying::hint("George Tucker"));
  assert(JazzNowPlaying::hint("Othello Molineaux"));
  assert(JazzNowPlaying::hint("Al Lucas"));
  assert(JazzNowPlaying::hint("Castor McCord"));
  assert(JazzNowPlaying::hint("Chris White"));
  assert(JazzNowPlaying::hint("Carl Morten Iversen"));
  assert(JazzNowPlaying::hint("Peter Beets"));
  assert(JazzNowPlaying::hint("Bing Crosby"));
  assert(JazzNowPlaying::hint("China Forbes"));
  assert(JazzNowPlaying::hint("Anthony Wilson"));
  assert(JazzNowPlaying::hint("Chris Pyne"));
  assert(JazzNowPlaying::hint("Bruce Ditmas"));
  assert(JazzNowPlaying::hint("Bert Boeren"));
  assert(JazzNowPlaying::hint("Bruce Barth"));
  assert(JazzNowPlaying::hint("Bruce Forman"));
  assert(JazzNowPlaying::hint("Barney McAll"));
  assert(JazzNowPlaying::hint("Chad Wackerman"));
  assert(JazzNowPlaying::hint("Axel Zwingenberger"));
  assert(JazzNowPlaying::hint("Candy Dulfer"));
  assert(JazzNowPlaying::hint("Dave Stryker"));
  assert(JazzNowPlaying::hint("Jacky Terrasson"));
  assert(JazzNowPlaying::hint("Holly Cole"));
  assert(JazzNowPlaying::hint("Champian Fulton"));
  assert(JazzNowPlaying::hint("Greg Abate"));
  assert(!JazzNowPlaying::hint("Holly Cole Trio"));
  assert(!JazzNowPlaying::hint("Dave Stryker Quartet"));
  assert(!JazzNowPlaying::hint("Chris Pyne Orchestra"));
  assert(!JazzNowPlaying::hint("Bruce Ditmas Quintet"));
  assert(!JazzNowPlaying::hint("Bert Boeren Quartet"));
  assert(!JazzNowPlaying::hint("Bing Crosby and the Rhythm Boys"));
  assert(!JazzNowPlaying::hint("Chris White Quintet"));
  assert(!JazzNowPlaying::hint("China Forbes with Pink Martini"));
  assert(!JazzNowPlaying::hint("Fred Guy Banjo"));
  assert(!JazzNowPlaying::hint("George Tucker Trio"));
  assert(!JazzNowPlaying::hint("Othello Molineaux Quartet"));
  assert(!JazzNowPlaying::hint("Nathan Davis Quintet"));
  assert(JazzNowPlaying::hint("Ann Burton"));
  assert(!JazzNowPlaying::hint("Mark Tokar Trio"));
  assert(!JazzNowPlaying::hint("Calvin Newborn Quartet"));
  assert(!JazzNowPlaying::hint("Buck Hill Trio"));
  assert(!JazzNowPlaying::hint("Cat Anderson Quartet"));
  assert(!JazzNowPlaying::hint("Alice Babs Sextet"));
  assert(!JazzNowPlaying::hint("Attila Zoller Trio"));
  assert(!JazzNowPlaying::hint("Charly Antolini Quartet"));
  assert(!JazzNowPlaying::hint("Carmen Lundy Quartet"));
  assert(!JazzNowPlaying::hint("Melba Liston Orchestra"));
  assert(strstr(JazzNowPlaying::hint("Miles Davis"), "modal"));
  assert(strstr(JazzNowPlaying::hint("Ella Fitzgerald"), "scat"));
  // Never infer an ambiguous artist from an approximate ICY title.
  assert(!JazzNowPlaying::hint(nullptr));
  assert(!JazzNowPlaying::hint(""));
  assert(!JazzNowPlaying::hint("Miles Davis Quintet"));
  assert(!JazzNowPlaying::hint("Miles Davis / John Coltrane"));
  assert(!JazzNowPlaying::hint("Unknown Artist"));
  assert(!JazzNowPlaying::hint("Miles"));
  puts("Curated offline Now Playing preview exact-match PASS");
}
